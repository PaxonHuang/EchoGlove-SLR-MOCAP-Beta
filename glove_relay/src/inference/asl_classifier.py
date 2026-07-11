# -*- coding: utf-8 -*-
"""glove_relay.src.inference.asl_classifier — ASL 6-letter template classifier.

Competition demo: classifies a 5-value flex vector into one of six ASL letters
(A/B/I/L/W/Y) via euclidean template matching, with PC-side dynamic-range
calibration (no firmware reflash).

Why this replaces the threshold ``rule_classifier`` for the demo:
  * The flex divider only spans ~0.90 V (straight) .. 2.15 V (bent) inside the
    ADC's ~0-2.5 V linear window, so the firmware's uncalibrated ``raw/4095``
    fallback compresses the signal to ~0.36..0.86 and the resting hand lands in
    a threshold dead-band → "无手势" forever. Capturing the real per-channel
    (min, max) and re-mapping ``(x-min)/(max-min)`` restores true 0..1
    (0=straight, 1=bent), matching the ASL spec convention.
  * The four active letters (A/B/I/L) are linearly separable in the 4-CHANNEL
    flex space (thumb/index/middle/pinky — the ring channel ch3 is a confirmed
    hardware fault and is masked out). Min inter-class euclidean distance on
    the 4 good channels = 0.923 (A vs L) >> sensor noise (~0.1), so nearest-
    template matching classifies them cleanly without a trained NN.

Flex convention (matches firmware + ASL spec):
    flex = [thumb, index, middle, ring, pinky]
    0.0 = straight   1.0 = bent

The firmware emits ``$EG,f0..f4,tick`` where f0..f4 are *uncalibrated*
``raw/4095`` values (~0.36..0.86). ``classify_raw`` re-normalizes using the
captured range before matching; ``classify_normalized`` assumes the caller
already normalized (used by tests / captured templates).
"""
from __future__ import annotations

import json
import math
from pathlib import Path
from typing import Dict, List, Optional, Tuple

# ── Gesture IDs ────────────────────────────────────────────────────────────
# 0 = 无手势 (no gesture) per web constants. 1..14 reserved for the existing
# Chinese-gesture web labels; 15..20 used by rule_classifier demo signs.
# ASL letters here use 21..26 so they never collide. The web frontend renders
# unknown IDs as "手势 #N" but demo_server sends ``nlp_text`` = the letter, so
# the display always shows the letter regardless.
GESTURE_IDS: Dict[str, int] = {"A": 21, "B": 22, "I": 23, "L": 24, "W": 25, "Y": 26}

# Display order kept stable for capture prompting + distance-matrix printing.
# NOTE: the active 4-channel demo uses only A/B/I/L (see ACTIVE_LETTERS). W/Y
# are retained in ASL_TEMPLATES for completeness but are NOT in the active set
# because (a) ch3 (ring) is a confirmed hardware fault on this glove and W/Y
# need the ring channel to separate from other letters, and (b) even on the 4
# good channels the W/Y templates collapse toward A/B/I under real capture
# noise. A/B/I/L has min inter-class distance 0.923 on the 4 good channels —
# the most robust 4-letter set for a broken-ring glove.
ACTIVE_LETTERS: Tuple[str, ...] = ("A", "B", "I", "L")
# Back-compat alias (calibrate_demo.py / tests import ASL_LETTERS). Now the
# 4-letter active set, not the old 6-letter set.
ASL_LETTERS: Tuple[str, ...] = ACTIVE_LETTERS

# ── Channel mask ───────────────────────────────────────────────────────────
# ch3 (ring, GPIO4) has a confirmed hardware fault (divider output pinned
# ~114 mV, ~0 swing across 6 diagnostic runs + a sensor swap). The classifier
# drops ch3 entirely and matches on the 4 surviving channels. Indices into the
# 5-element flex vector: [thumb, index, middle, ring, pinky] → keep 0,1,2,4.
ACTIVE_CHANNELS: Tuple[int, ...] = (0, 1, 2, 4)
ACTIVE_DIM: int = len(ACTIVE_CHANNELS)

# ── Theoretical templates (ASL spec §2 / §3.1) ─────────────────────────────
# flex = [thumb, index, middle, ring, pinky], 0=straight 1=bent. Full 5-D
# templates retained for reference; matching projects onto ACTIVE_CHANNELS.
ASL_TEMPLATES: Dict[str, List[float]] = {
    "A": [0.30, 0.95, 1.00, 1.00, 0.95],
    "B": [0.60, 0.05, 0.05, 0.05, 0.05],
    "I": [0.60, 0.95, 1.00, 1.00, 0.05],
    "L": [0.10, 0.05, 0.95, 1.00, 0.95],
    "W": [0.60, 0.05, 0.05, 0.05, 0.85],
    "Y": [0.05, 0.90, 0.95, 0.95, 0.10],
}

# On the 4 active channels (drop ring), the min inter-class euclidean distance
# over the ACTIVE_LETTERS theoretical templates is 0.923 (A vs L), far above
# the 5-channel value of 0.559 (I vs Y) — dropping the broken ring channel
# actually IMPROVES separability for the 4 chosen letters. Reject threshold
# sits at ~1/3 of that span; borderline matches still classify, genuinely far
# / unknown poses (>0.35) reject.
DEFAULT_REJECT_THRESHOLD = 0.35
# confidence = 1 - dist / CONF_SPAN; using the active-set min class distance
# (0.923) → a perfect match scores 1.0, the closest confusable pair ~0.0.
CONF_SPAN = 0.923

DEFAULT_CALIBRATION_PATH = Path(__file__).resolve().parents[2] / "configs" / "demo_calibration.json"


def _clamp01(v: float) -> float:
    return 0.0 if v < 0.0 else (1.0 if v > 1.0 else v)


class ASLClassifier:
    """Euclidean nearest-template classifier with dynamic-range calibration.

    State:
      * ``range_min`` / ``range_max`` — per-channel raw (``raw/4095``-scale)
        values captured at open hand / fist. ``None`` until calibrated.
      * ``polarity`` — +1 if raw rises when bent (typical), -1 if inverted.
      * ``captured_templates`` — per-letter mean normalized vector from a live
        capture session. Used in preference to the theoretical templates when
        ``use_capt`` is True.
    """

    def __init__(self, reject_threshold: float = DEFAULT_REJECT_THRESHOLD) -> None:
        self.reject_threshold = reject_threshold
        # Range/polarity are kept full 5-D (the $EG stream is 5 channels) but
        # only the ACTIVE_CHANNELS entries are used for normalization + match.
        self.range_min: Optional[List[float]] = None
        self.range_max: Optional[List[float]] = None
        self.polarity: List[int] = [1, 1, 1, 1, 1]
        # Captured templates are stored ALREADY projected to ACTIVE_CHANNELS
        # (length ACTIVE_DIM), so load/save + classify all stay in the masked
        # space consistently.
        self.captured_templates: Dict[str, List[float]] = {}
        self.use_capt: bool = False

    # ── projection onto the active (working) channels ─────────────────────
    @staticmethod
    def _project(vec: List[float]) -> List[float]:
        """Keep only the ACTIVE_CHANNELS dimensions of a 5-vector."""
        v = [float(x) for x in vec]
        while len(v) < 5:
            v.append(0.0)
        return [v[i] for i in ACTIVE_CHANNELS]

    # ── calibration state ────────────────────────────────────────────────
    def set_range(self, min: List[float], max: List[float],
                  polarity: Optional[List[int]] = None) -> None:
        assert len(min) == 5 and len(max) == 5
        self.range_min = [float(x) for x in min]
        self.range_max = [float(x) for x in max]
        if polarity is not None:
            assert len(polarity) == 5
            self.polarity = [int(p) if p in (1, -1) else 1 for p in polarity]

    def set_captured_templates(self, t: Dict[str, List[float]]) -> None:
        # Accept either full 5-D or already-projected templates; normalize to
        # the projected length ACTIVE_DIM so classify + distance are consistent.
        proj = {}
        for k, v in t.items():
            v = [float(x) for x in v]
            if len(v) == 5:
                v = self._project(v)
            elif len(v) != ACTIVE_DIM:
                v = (v + [0.0] * ACTIVE_DIM)[:ACTIVE_DIM]
            proj[k] = v
        self.captured_templates = proj

    def use_captured(self, flag: bool) -> None:
        self.use_capt = flag and bool(self.captured_templates)

    @property
    def has_range(self) -> bool:
        return self.range_min is not None and self.range_max is not None

    def _active_templates(self) -> Dict[str, List[float]]:
        """Templates projected onto ACTIVE_CHANNELS (length ACTIVE_DIM).

        When using captured templates they are already projected (stored that
        way by set_captured_templates). When using theoretical templates the
        full 5-D ASL_TEMPLATES are projected here on the fly. Only ACTIVE_LETTERS
        are returned so W/Y (which need the broken ring channel) never match.
        """
        if self.use_capt and self.captured_templates:
            return {k: v for k, v in self.captured_templates.items() if k in ACTIVE_LETTERS}
        return {L: self._project(ASL_TEMPLATES[L]) for L in ACTIVE_LETTERS}

    # ── normalization ────────────────────────────────────────────────────
    def normalize(self, raw5: List[float]) -> List[float]:
        """Re-map a raw ``raw/4095``-scale 5-vector to true 0..1 (0=straight,
        1=bent), projected onto ACTIVE_CHANNELS (length ACTIVE_DIM).

        The broken ring channel (ch3) is dropped here — its range/polarity
        entry is ignored. Without calibration, returns the projected input
        clamped to [0, 1].
        """
        f = [float(x) for x in raw5]
        while len(f) < 5:
            f.append(0.0)
        f = f[:5]
        if not self.has_range:
            return [_clamp01(f[i]) for i in ACTIVE_CHANNELS]
        out = []
        for i in ACTIVE_CHANNELS:
            span = self.range_max[i] - self.range_min[i]
            if abs(span) < 1e-6:
                n = 0.0
            else:
                n = (f[i] - self.range_min[i]) / span
            if self.polarity[i] == -1:
                n = 1.0 - n
            out.append(_clamp01(n))
        return out

    # ── capture helpers (used by calibrate_demo.py) ──────────────────────
    @staticmethod
    def _mean(samples: List[List[float]]) -> List[float]:
        """Per-channel mean. Samples are ACTIVE_DIM-length projected vectors."""
        if not samples:
            return [0.0] * ACTIVE_DIM
        n = len(samples)
        dim = len(samples[0])
        return [sum(s[i] for s in samples) / n for i in range(dim)]

    def capture_letter(self, letter: str, raw_samples: List[List[float]]) -> List[float]:
        """Record a per-letter template from several raw 5-D frames.

        Stores the *normalized, projected* mean (length ACTIVE_DIM) so the
        template is independent of the raw scale and of the broken ring ch.
        Range must be set first.
        """
        if not self.has_range:
            raise RuntimeError("set_range() before capture_letter()")
        norm = [self.normalize(s) for s in raw_samples]
        tmpl = self._mean(norm)
        self.captured_templates[letter] = tmpl
        return tmpl

    @staticmethod
    def detect_polarity(open_mean: List[float], fist_mean: List[float]) -> List[int]:
        """+1 if the channel rises open→fist, -1 if it drops."""
        return [1 if fist_mean[i] >= open_mean[i] else -1 for i in range(5)]

    # ── classification ───────────────────────────────────────────────────
    def classify_normalized(self, norm5: List[float]) -> Dict:
        """Classify an already-normalized vector. Accepts either a full 5-D
        vector (projected internally) or an ACTIVE_DIM-length projected vector.
        Returns a result dict.
        """
        f = [float(x) for x in norm5]
        # If given a full 5-D vector, project; otherwise assume already projected.
        if len(f) == 5:
            f = self._project(f)
        elif len(f) != ACTIVE_DIM:
            f = (f + [0.0] * ACTIVE_DIM)[:ACTIVE_DIM]
        f = [_clamp01(x) for x in f]

        best_letter: Optional[str] = None
        best_dist = float("inf")
        second_dist = float("inf")
        for letter, tmpl in self._active_templates().items():
            d = math.dist(f, tmpl)
            if d < best_dist:
                second_dist = best_dist
                best_dist = d
                best_letter = letter
            elif d < second_dist:
                second_dist = d

        confidence = max(0.0, 1.0 - best_dist / CONF_SPAN)
        if best_dist > self.reject_threshold or best_letter is None:
            return {"letter": None, "label": "unknown", "text": "无手势",
                    "gesture_id": 0, "confidence": round(confidence, 3),
                    "distance": round(best_dist, 3)}
        # Margin bonus: a clean match (nearest << second) lifts confidence.
        margin = second_dist - best_dist
        confidence = min(1.0, confidence + 0.1 * (margin / CONF_SPAN))
        gid = GESTURE_IDS[best_letter]
        return {"letter": best_letter, "label": f"asl_{best_letter.lower()}",
                "text": best_letter, "gesture_id": gid,
                "confidence": round(confidence, 3), "distance": round(best_dist, 3)}

    def classify_raw(self, raw5: List[float]) -> Dict:
        """Classify a raw ``raw/4095``-scale vector (the $EG stream)."""
        return self.classify_normalized(self.normalize(raw5))

    # ── persistence ──────────────────────────────────────────────────────
    def save(self, path: Path) -> None:
        path = Path(path)
        path.parent.mkdir(parents=True, exist_ok=True)
        data = {
            "version": 2,
            "active_letters": list(ACTIVE_LETTERS),
            "active_channels": list(ACTIVE_CHANNELS),
            "active_channel_names": ["thumb", "index", "middle", "pinky"],
            "disabled_channel": 3,  # ring (GPIO4) — hardware fault, dropped
            "convention": {"channels": ["thumb", "index", "middle", "ring", "pinky"],
                           "active": ["thumb", "index", "middle", "pinky"],
                           "scale": "0=straight, 1=bent"},
            "range": {"min": self.range_min, "max": self.range_max},
            "polarity": self.polarity,
            "captured_templates": self.captured_templates,  # length ACTIVE_DIM
            "use_captured": self.use_capt,
            "reject_threshold": self.reject_threshold,
        }
        path.write_text(json.dumps(data, indent=2, ensure_ascii=False), encoding="utf-8")

    def load(self, path: Path) -> bool:
        path = Path(path)
        if not path.exists():
            return False
        try:
            data = json.loads(path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError):
            return False
        rng = data.get("range") or {}
        if rng.get("min") and rng.get("max"):
            self.set_range(rng["min"], rng["max"], data.get("polarity"))
        cap = data.get("captured_templates") or {}
        if cap:
            self.set_captured_templates(cap)
        self.use_captured(bool(data.get("use_captured", False)))
        if "reject_threshold" in data:
            self.reject_threshold = float(data["reject_threshold"])
        return True

    # ── diagnostics ──────────────────────────────────────────────────────
    def distance_matrix(self) -> str:
        """Printable inter-template euclidean distance matrix (sanity check).

        Computed in the projected ACTIVE_CHANNELS space (length ACTIVE_DIM),
        matching what classify actually compares.
        """
        tmpls = self._active_templates()
        letters = [L for L in ACTIVE_LETTERS if L in tmpls]
        header = "    " + " ".join(f"{L:>6}" for L in letters)
        lines = [header]
        for a in letters:
            row = [f"{math.dist(tmpls[a], tmpls[b]):6.2f}" for b in letters]
            lines.append(f" {a}  " + " ".join(row))
        # Report the minimum off-diagonal distance.
        off = [math.dist(tmpls[a], tmpls[b])
               for i, a in enumerate(letters) for b in letters[i + 1:]]
        if off:
            lines.append(f"min inter-class distance = {min(off):.3f}"
                         f"  (reject threshold = {self.reject_threshold})")
        return "\n".join(lines)
