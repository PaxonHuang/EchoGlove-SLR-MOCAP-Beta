"""V5 Three-tier Confidence Router with hot-swap blending."""
from __future__ import annotations
from typing import Optional
from collections import deque


class ConfidenceRouter:
    def __init__(self, blend_frames: int = 5, min_confidence: float = 0.5):
        self.blend_frames = blend_frames
        self.min_confidence = min_confidence
        self.active_tier = "tier1"
        self.blend_counter = 0
        self.blend_source = None
        self.blend_target = None
        self.history = deque(maxlen=10)

    def route(
        self,
        tier1: Optional[dict] = None,
        tier2: Optional[dict] = None,
        tier3: Optional[dict] = None,
    ) -> dict:
        best = tier1 or {"gesture_id": 0, "confidence": 0.0}
        best_tier = "tier1"

        if tier2 and tier2["confidence"] >= self.min_confidence:
            best = tier2
            best_tier = "tier2"

        if tier3 and tier3["confidence"] >= self.min_confidence:
            best = tier3
            best_tier = "tier3"

        if best_tier != self.active_tier:
            self.blend_source = self.active_tier
            self.blend_target = best_tier
            self.blend_counter = 0
            self.active_tier = best_tier

        alpha = 1.0
        if self.blend_counter < self.blend_frames and self.blend_source:
            alpha = self.blend_counter / self.blend_frames
            self.blend_counter += 1
            if self.blend_counter >= self.blend_frames:
                self.blend_source = None

        self.history.append(best)
        return {
            "gesture_id": best["gesture_id"],
            "confidence": best["confidence"],
            "active_tier": self.active_tier,
            "blend_alpha": alpha,
        }
