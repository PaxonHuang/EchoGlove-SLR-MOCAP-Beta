# -*- coding: utf-8 -*-
"""glove_relay.src.inference.rule_classifier — Rule-based sign classifier.

Competition demo fast-path: classifies a 5-value flex vector into a small set
of demo signs via thresholds, without any trained NN model.

Flex convention (verified in firmware, see [[demo-fast-prototype-plan]]):
    flex = [thumb, index, middle, ring, pinky]
    0.0 = straight   1.0 = bent   (normalized by FlexManager via NVS calibration;
                                   uncalibrated fallback = raw / 4095)

Output gesture IDs align with ``glove_web/src/utils/constants.ts``
``GESTURE_LABELS`` where possible, with new IDs 15+ for demo-only signs.
"""
from __future__ import annotations

from typing import Dict, List, Optional


# ── Gesture IDs ────────────────────────────────────────────────────────────
# 0 = 无手势 (no gesture), 1..14 = existing web GESTURE_LABELS.
# Demo-only signs start at 15 (not in web constants → shown as "手势 #N", still
# rendered; nlp_text carries the Chinese word so it is always legible).
GESTURE_IDS = {
    "open_hand": 0,
    "fist": 13,       # GESTURE_LABELS[13] = '好'
    "thumbs_up": 15,
    "point": 16,
    "peace": 17,
    "three": 18,
    "four": 19,
    "pinky_up": 20,
}

GESTURE_TEXT = {
    "open_hand": "无手势",
    "fist": "好",
    "thumbs_up": "赞",
    "point": "一",
    "peace": "二",
    "three": "三",
    "four": "四",
    "pinky_up": "五",
}


def _flex(flex: List[float]) -> List[float]:
    """Normalize to a 5-list of floats clamped to [0, 1]."""
    f = [float(x) if x is not None else 0.0 for x in flex]
    while len(f) < 5:
        f.append(0.0)
    return [max(0.0, min(1.0, x)) for x in f[:5]]


class RuleClassifier:
    """Threshold-based classifier over 5 flex channels.

    Each rule is (name, list_of_(indices, want_bent)) + min_match fraction.
    A finger is "bent" if flex >= ``bent`` (default 0.6), else "straight"
    if flex <= ``straight`` (default 0.4); values in the dead band count as
    neither and fail the "straight" requirement for fingers expected straight.
    """

    def __init__(self, bent: float = 0.6, straight: float = 0.4) -> None:
        if bent <= straight:
            raise ValueError("bent threshold must be > straight threshold")
        self.bent = bent
        self.straight = straight

    def classify(self, flex: List[float]) -> Dict:
        """Classify a 5-value flex vector.

        Returns a dict with keys: ``gesture_id`` (int, -1 if no rule matched),
        ``label`` (str key), ``text`` (Chinese display text),
        ``confidence`` (float 0..1, higher = cleaner match).
        """
        f = _flex(flex)
        thumb, index, middle, ring, pinky = f

        def is_bent(v: float) -> bool:
            return v >= self.bent

        def is_straight(v: float) -> bool:
            return v <= self.straight

        all_bent = all(is_bent(v) for v in f)
        all_straight = all(is_straight(v) for v in f)

        # Fist — all bent. GESTURE_LABELS[13] = '好'
        if all_bent:
            return self._result("fist", confidence=self._conf(f, bent_fingers=[0, 1, 2, 3, 4]))

        # Open hand — all straight → rest / no gesture
        if all_straight:
            return self._result("open_hand", confidence=self._conf(f, straight_fingers=[0, 1, 2, 3, 4]))

        # Thumbs up — thumb straight, others bent
        if is_straight(thumb) and is_bent(index) and is_bent(middle) and is_bent(ring) and is_bent(pinky):
            return self._result("thumbs_up", confidence=self._conf(f, straight_fingers=[0], bent_fingers=[1, 2, 3, 4]))

        # Point (一) — index straight, others bent
        if is_straight(index) and is_bent(thumb) and is_bent(middle) and is_bent(ring) and is_bent(pinky):
            return self._result("point", confidence=self._conf(f, straight_fingers=[1], bent_fingers=[0, 2, 3, 4]))

        # Peace / victory (二) — index + middle straight, others bent
        if is_straight(index) and is_straight(middle) and is_bent(thumb) and is_bent(ring) and is_bent(pinky):
            return self._result("peace", confidence=self._conf(f, straight_fingers=[1, 2], bent_fingers=[0, 3, 4]))

        # Three (三) — index + middle + ring straight, thumb + pinky bent
        if is_straight(index) and is_straight(middle) and is_straight(ring) and is_bent(thumb) and is_bent(pinky):
            return self._result("three", confidence=self._conf(f, straight_fingers=[1, 2, 3], bent_fingers=[0, 4]))

        # Four (四) — four fingers straight, thumb bent
        if is_straight(index) and is_straight(middle) and is_straight(ring) and is_straight(pinky) and is_bent(thumb):
            return self._result("four", confidence=self._conf(f, straight_fingers=[1, 2, 3, 4], bent_fingers=[0]))

        # Pinky up / 五 — pinky straight, others bent
        if is_straight(pinky) and is_bent(thumb) and is_bent(index) and is_bent(middle) and is_bent(ring):
            return self._result("pinky_up", confidence=self._conf(f, straight_fingers=[4], bent_fingers=[0, 1, 2, 3]))

        # No rule matched (partial / ambiguous pose) → no gesture, low conf.
        return {"gesture_id": 0, "label": "unknown", "text": "无手势", "confidence": 0.2}

    # ── helpers ────────────────────────────────────────────────────────────
    def _result(self, label: str, confidence: float) -> Dict:
        return {
            "gesture_id": GESTURE_IDS[label],
            "label": label,
            "text": GESTURE_TEXT[label],
            "confidence": round(max(0.0, min(1.0, confidence)), 3),
        }

    def _conf(
        self,
        f: List[float],
        bent_fingers: Optional[List[int]] = None,
        straight_fingers: Optional[List[int]] = None,
    ) -> float:
        """Confidence from how far each finger clears its threshold.

        Bent fingers contribute (v - bent)/(1 - bent); straight fingers
        contribute (straight - v)/straight. Averaged and lifted so a clean
        0/1 pose (fingers fully at the far end) scores ~1.0.
        """
        bent_fingers = bent_fingers or []
        straight_fingers = straight_fingers or []
        scores: List[float] = []
        span_b = max(1e-6, 1.0 - self.bent)
        span_s = max(1e-6, self.straight)
        for i in bent_fingers:
            scores.append(min(1.0, (f[i] - self.bent) / span_b))
        for i in straight_fingers:
            scores.append(min(1.0, (self.straight - f[i]) / span_s))
        if not scores:
            return 0.5
        # Base = average clearance; lift so a perfect pose (avg≈1) → 0.99.
        avg = sum(scores) / len(scores)
        return 0.6 + 0.39 * avg
