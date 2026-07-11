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
  * The six letters are linearly separable in 5-D flex space (min inter-class
    euclidean distance = 0.80, B vs W) >> sensor noise (~0.1), so nearest-
    template matching classifies them cleanly without a trained NN. (See the
    ASL spec §3.2.)

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
ASL_LETTERS: Tuple[str, ...] = ("A", "B", "I", "L", "W", "Y")

# ── Theoretical templates (ASL spec §2 / §3.1) ─────────────────────────────
# flex = [thumb, index, middle, ring, pinky], 0=straight 1=bent
ASL_TEMPLATES: Dict[str, List[float]] = {
    "A": [0.30, 0.95, 1.00, 1.00, 0.95],
    "B": [0.60, 0.05, 0.05, 0.05, 0.05],
    "I": [0.60, 0.95, 1.00, 1.00, 0.05],
    "L": [0.10, 0.05, 0.95, 1.00, 0.95],
    "W": [0.60, 0.05, 0.05, 0.05, 0.85],
    "Y": [0.05, 0.90, 0.95, 0.95, 0.10],
}

# Actual min inter-class euclidean distance over the six theoretical templates
# is 0.559 (I vs Y) — NOT 0.80 (B vs W) as the ASL spec §3.2 table claims; the
# spec's distance table is internally inconsistent (it lists A-B=1.64 but the
# real value is 1.875). We use the code-computed value. The closest pair I/Y
# differ mainly on thumb (0.60 vs 0.05) + pinky (0.05 vs 0.10); their midpoint
# sits ~0.28 from each, so a reject threshold of 0.34 lets borderline I/Y
# readings classify while rejecting genuinely far/unknown poses (>0.34). With
# per-user captured templates the real min distance is usually larger.
DEFAULT_REJECT_THRESHOLD = 0.34
# confidence = 1 - dist / CONF_SPAN; using the real min class distance (0.559)
# → a perfect match scores 1.0 and the closest confusable pair scores ~0.0.
CONF_SPAN = 0.559

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
        self.range_min: Optional[List[float]] = None
        self.range_max: Optional[List[float]] = None
        self.polarity: List[int] = [1, 1, 1, 1, 1]
        self.captured_templates: Dict[str, List[float]] = {}
        self.use_capt: bool = False

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
        self.captured_templates = {k: [float(x) for x in v] for k, v in t.items()}

    def use_captured(self, flag: bool) -> None:
        self.use_capt = flag and bool(self.captured_templates)

    @property
    def has_range(self) -> bool:
        return self.range_min is not None and self.range_max is not None

    def _active_templates(self) -> Dict[str, List[float]]:
        if self.use_capt and self.captured_templates:
            return self.captured_templates
        return ASL_TEMPLATES

    # ── normalization ────────────────────────────────────────────────────
    def normalize(self, raw5: List[float]) -> List[float]:
        """Re-map a raw ``raw/4095``-scale vector to true 0..1 (0=straight,
        1=bent) using the captured per-channel range + polarity.

        Without calibration, returns the input clamped to [0, 1] (caller then
        gets the compressed ~0.36..0.86 values — classification will still run
        against theoretical templates but accuracy suffers).
        """
        f = [float(x) for x in raw5]
        while len(f) < 5:
            f.append(0.0)
        f = f[:5]
        if not self.has_range:
            return [_clamp01(x) for x in f]
        out = []
        for i, v in enumerate(f):
            span = self.range_max[i] - self.range_min[i]
            if abs(span) < 1e-6:
                n = 0.0
            else:
                n = (v - self.range_min[i]) / span
            if self.polarity[i] == -1:
                n = 1.0 - n
            out.append(_clamp01(n))
        return out

    # ── capture helpers (used by calibrate_demo.py) ──────────────────────
    @staticmethod
    def _mean(samples: List[List[float]]) -> List[float]:
        n = len(samples)
        return [sum(s[i] for s in samples) / n for i in range(5)]

    def capture_letter(self, letter: str, raw_samples: List[List[float]]) -> List[float]:
        """Record a per-letter template from several raw frames.

        Stores the *normalized* mean so the template is independent of the raw
        scale. Range must be set first.
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
        """Classify an already-normalized 5-vector. Returns a result dict."""
        f = [_clamp01(float(x)) for x in norm5]
        while len(f) < 5:
            f.append(0.0)
        f = f[:5]

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
            "version": 1,
            "convention": {"channels": ["thumb", "index", "middle", "ring", "pinky"],
                           "scale": "0=straight, 1=bent"},
            "range": {"min": self.range_min, "max": self.range_max},
            "polarity": self.polarity,
            "captured_templates": self.captured_templates,
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
        """Printable inter-template euclidean distance matrix (sanity check)."""
        tmpls = self._active_templates()
        letters = [L for L in ASL_LETTERS if L in tmpls]
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
