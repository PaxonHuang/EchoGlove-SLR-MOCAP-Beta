# -*- coding: utf-8 -*-
from __future__ import annotations

"""
glove_relay.src.mock_data — Software-only mock data source.

Generates V5 dual-hand sensor messages and broadcasts them to the WebSocket
manager.  Used for frontend / integration testing when real glove hardware is
not available.

The output schema matches ``tests/test_ws_format_v5.py`` and is consumed
natively by ``glove_web/src/stores/useSensorStore.ts``.
"""

import asyncio
import math
import random
import time
from dataclasses import dataclass
from typing import Callable, Coroutine, List

from src.utils.logger import get_logger

logger = get_logger(__name__)


# ---------------------------------------------------------------------------
# Predefined gesture poses
# ---------------------------------------------------------------------------
@dataclass(frozen=True)
class _GesturePose:
    name: str
    left_flex: tuple[float, ...]
    right_flex: tuple[float, ...]


# Flex order: thumb, index, middle, ring, pinky
_GESTURES: tuple[_GesturePose, ...] = (
    _GesturePose("open_hand", (0.1, 0.1, 0.1, 0.1, 0.1), (0.1, 0.1, 0.1, 0.1, 0.1)),
    _GesturePose("fist", (0.9, 0.9, 0.9, 0.9, 0.9), (0.9, 0.9, 0.9, 0.9, 0.9)),
    _GesturePose("thumbs_up", (0.1, 0.9, 0.9, 0.9, 0.9), (0.1, 0.9, 0.9, 0.9, 0.9)),
    _GesturePose("peace", (0.6, 0.1, 0.1, 0.9, 0.9), (0.6, 0.1, 0.1, 0.9, 0.9)),
    _GesturePose("point", (0.5, 0.1, 0.9, 0.9, 0.9), (0.5, 0.1, 0.9, 0.9, 0.9)),
)


# ---------------------------------------------------------------------------
# Mock data source
# ---------------------------------------------------------------------------
class MockDataSource:
    """Produce synthetic V5 sensor messages at a fixed frame rate."""

    def __init__(
        self,
        fps: float = 30.0,
        gesture_cycle_seconds: float = 3.0,
        noise_std: float = 0.02,
        seed: int | None = None,
    ) -> None:
        """
        Parameters
        ----------
        fps:
            Messages per second.
        gesture_cycle_seconds:
            Seconds spent on each gesture before switching to the next.
        noise_std:
            Standard deviation of Gaussian noise added to flex values.
        seed:
            Optional random seed for reproducible tests.
        """
        self._fps = max(1.0, fps)
        self._cycle_seconds = max(0.5, gesture_cycle_seconds)
        self._noise_std = max(0.0, noise_std)
        self._rng = random.Random(seed)
        self._running = False
        self._task: asyncio.Task[None] | None = None

    # ------------------------------------------------------------------
    # Public API
    # ------------------------------------------------------------------
    def start(
        self,
        broadcast: Callable[[dict], Coroutine[None, None, None]],
    ) -> asyncio.Task[None]:
        """Start the background mock-data task."""
        if self._running:
            raise RuntimeError("MockDataSource is already running")
        self._running = True
        self._task = asyncio.create_task(self._run(broadcast), name="mock-data")
        logger.info(
            "Mock data source started (fps=%.1f, cycle=%.1fs)",
            self._fps,
            self._cycle_seconds,
        )
        return self._task

    async def stop(self) -> None:
        """Stop the background task and wait for it to finish."""
        self._running = False
        if self._task is not None:
            self._task.cancel()
            try:
                await self._task
            except asyncio.CancelledError:
                pass
            self._task = None
        logger.info("Mock data source stopped")

    # ------------------------------------------------------------------
    # Internal loop
    # ------------------------------------------------------------------
    async def _run(
        self,
        broadcast: Callable[[dict], Coroutine[None, None, None]],
    ) -> None:
        """Generate and broadcast messages until ``stop()`` is called."""
        interval = 1.0 / self._fps
        start_time = time.monotonic()
        frame = 0

        while self._running:
            now = time.monotonic()
            msg = self._build_message(now - start_time, frame)
            try:
                await broadcast(msg)
            except Exception:
                logger.exception("Mock data broadcast failed")

            frame += 1
            # Sleep until next frame, accounting for processing time
            next_time = start_time + frame * interval
            delay = next_time - time.monotonic()
            if delay > 0:
                await asyncio.sleep(delay)

    def _build_message(self, elapsed: float, frame: int) -> dict:
        """Create one V5-format message."""
        cycle_idx = int(elapsed // self._cycle_seconds) % len(_GESTURES)
        next_cycle_idx = (cycle_idx + 1) % len(_GESTURES)
        alpha = (elapsed % self._cycle_seconds) / self._cycle_seconds

        current = _GESTURES[cycle_idx]
        nxt = _GESTURES[next_cycle_idx]

        # Smooth interpolation between gestures
        left_flex = self._interp(current.left_flex, nxt.left_flex, alpha)
        right_flex = self._interp(current.right_flex, nxt.right_flex, alpha)

        # Add small noise so the 3D hand appears alive
        left_flex = self._add_noise(left_flex)
        right_flex = self._add_noise(right_flex)

        # Slow wrist movement for visual feedback
        euler_l = [
            10.0 * math.sin(elapsed * 0.5),
            20.0 * math.cos(elapsed * 0.3),
            30.0 + 10.0 * math.sin(elapsed * 0.7),
        ]
        euler_r = [
            -10.0 * math.sin(elapsed * 0.5),
            25.0 * math.cos(elapsed * 0.3),
            -30.0 + 10.0 * math.cos(elapsed * 0.7),
        ]
        gyro_l = [
            1.0 * math.cos(elapsed),
            2.0 * math.sin(elapsed * 1.2),
            3.0 * math.cos(elapsed * 0.8),
        ]
        gyro_r = [
            -1.0 * math.cos(elapsed),
            -2.0 * math.sin(elapsed * 1.2),
            -3.0 * math.cos(elapsed * 0.8),
        ]

        # Gesture IDs are stable per pose
        gesture_id = cycle_idx
        confidence = 0.85 + 0.1 * math.sin(elapsed * 2.0)

        return {
            "tier": "tier2",
            "blend_alpha": 1.0,
            "left_hand": {
                "flex": left_flex,
                "euler": euler_l,
                "gyro": gyro_l,
                "gesture_id": gesture_id,
                "confidence": round(confidence, 3),
            },
            "right_hand": {
                "flex": right_flex,
                "euler": euler_r,
                "gyro": gyro_r,
                "gesture_id": gesture_id,
                "confidence": round(confidence, 3),
            },
            "relative": {
                "delta_euler": [0.1, 0.2, 0.3],
                "delta_quat_dist": 0.01,
                "delta_gyro_norm": 0.5,
                "delta_gyro_axis": 0.33,
            },
            "inference": {
                "gesture_id": gesture_id,
                "confidence": round(confidence, 3),
                "text": current.name,
            },
            "nlp_text": current.name,
        }

    # ------------------------------------------------------------------
    # Helpers
    # ------------------------------------------------------------------
    def _interp(
        self,
        a: tuple[float, ...],
        b: tuple[float, ...],
        alpha: float,
    ) -> List[float]:
        """Linear interpolation between two pose vectors."""
        alpha = max(0.0, min(1.0, alpha))
        return [x * (1.0 - alpha) + y * alpha for x, y in zip(a, b)]

    def _add_noise(self, values: List[float]) -> List[float]:
        """Clamp Gaussian noise to keep values in [0, 1]."""
        if self._noise_std <= 0.0:
            return values
        return [max(0.0, min(1.0, v + self._rng.gauss(0.0, self._noise_std))) for v in values]
