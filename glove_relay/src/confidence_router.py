# -*- coding: utf-8 -*-
from __future__ import annotations

"""
glove_relay.src.confidence_router — Confidence-driven L1→L2 routing logic.

Extracted from UDPServer for testability.  The router decides whether to:
  - Emit an L1 result directly (high confidence).
  - Buffer frames and trigger L2 inference (low confidence + debounce + silence).
  - Degrade to UNKNOWN on L2 timeout.
"""

import time
from collections import deque
from dataclasses import dataclass, field
from typing import Any, Callable, Deque, Optional

from src.utils.config import get_config
from src.utils.logger import get_logger

logger = get_logger(__name__)


@dataclass
class RouteResult:
    """Output of a single routing decision."""
    gesture_id: int = -1
    confidence: float = 0.0
    source: str = "unknown"        # "l1" | "l2" | "unknown"
    nlp_text: str = ""
    tts_audio_b64: str = ""
    status: str = "unknown"        # "l1_ok" | "l2_ok" | "l2_timeout" | "unknown"


class ConfidenceRouter:
    """
    Decides whether to trust L1 or trigger L2 inference.

    Parameters
    ----------
    config :
        Relay configuration (uses ``inference`` section).
    l1_model :
        Callable that takes ``(features: np.ndarray) -> (gesture_id, confidence)``.
    l2_model :
        Callable that takes ``(window: np.ndarray) -> (gesture_id, confidence)``.
    grammar_corrector :
        Optional callable ``(gesture_ids: list[int]) -> str``.
    tts_engine :
        Optional async callable ``(text: str) -> bytes``.
    """

    def __init__(
        self,
        config: Any = None,
        l1_model: Optional[Callable] = None,
        l2_model: Optional[Callable] = None,
        grammar_corrector: Optional[Callable] = None,
        tts_engine: Optional[Callable] = None,
    ) -> None:
        cfg = config or get_config()
        self._threshold: float = cfg.inference.l1_confidence_threshold
        self._debounce_frames: int = cfg.inference.debounce_frames
        self._silence_ms: int = cfg.inference.gesture_silence_ms
        self._window_size: int = cfg.inference.l2_window_size
        self._l2_enabled: bool = cfg.inference.l2_enabled

        self._l1_model = l1_model
        self._l2_model = l2_model
        self._grammar_corrector = grammar_corrector
        self._tts_engine = tts_engine

        self._frame_buffer: Deque[dict[str, Any]] = deque(maxlen=self._window_size)
        self._debounce_counter: int = 0
        self._last_gesture_time: float = 0.0
        self._last_gesture_id: int = -1

    # ------------------------------------------------------------------
    # Public API
    # ------------------------------------------------------------------
    def route(self, parsed: dict[str, Any]) -> RouteResult:
        """
        Route a single sensor frame through L1 (and optionally L2).

        Parameters
        ----------
        parsed :
            Parsed protobuf dict with keys ``hall``, ``imu``, ``l1_gesture_id``,
            ``l1_confidence``, etc.

        Returns
        -------
        RouteResult
            The routing decision including gesture ID, confidence, and source.
        """
        # --- L1 inference ---
        l1_id, l1_conf = self._run_l1(parsed)

        result = RouteResult(
            gesture_id=l1_id,
            confidence=l1_conf,
            source="l1",
            status="l1_ok",
        )

        # --- High confidence: emit directly ---
        if l1_conf > self._threshold:
            self._debounce_counter = 0
            self._apply_nlp(result)
            return result

        # --- Low confidence: try L2 ---
        if not self._l2_enabled:
            self._apply_nlp(result)
            return result

        now = time.time() * 1000.0  # ms
        self._frame_buffer.append(parsed)
        self._debounce_counter += 1

        # Check debounce + silence
        if (
            self._debounce_counter >= self._debounce_frames
            and (now - self._last_gesture_time) >= self._silence_ms
            and len(self._frame_buffer) >= self._window_size
        ):
            l2_id, l2_conf = self._run_l2()
            if l2_id >= 0:
                result.gesture_id = l2_id
                result.confidence = l2_conf
                result.source = "l2"
                result.status = "l2_ok"
            else:
                result.status = "l2_timeout"
            self._frame_buffer.clear()
            self._debounce_counter = 0
            self._last_gesture_time = now
            self._last_gesture_id = result.gesture_id
            self._apply_nlp(result)
            return result

        # Not enough frames yet — return L1 with low confidence
        self._apply_nlp(result)
        return result

    def reset(self) -> None:
        """Clear all internal state."""
        self._frame_buffer.clear()
        self._debounce_counter = 0
        self._last_gesture_time = 0.0
        self._last_gesture_id = -1

    @property
    def buffer_size(self) -> int:
        return len(self._frame_buffer)

    # ------------------------------------------------------------------
    # Internal
    # ------------------------------------------------------------------
    def _run_l1(self, parsed: dict[str, Any]) -> tuple[int, float]:
        if self._l1_model is not None:
            import numpy as np
            features = np.array(
                parsed.get("hall", []) + parsed.get("imu", []),
                dtype=np.float32,
            )
            return self._l1_model(features.reshape(1, -1))
        return parsed.get("l1_gesture_id", -1), parsed.get("l1_confidence", 0.0)

    def _run_l2(self) -> tuple[int, float]:
        if self._l2_model is not None:
            import numpy as np
            window = np.stack(
                [np.array(f.get("hall", []) + f.get("imu", []), dtype=np.float32)
                 for f in self._frame_buffer],
                axis=0,
            )
            return self._l2_model(window.reshape(1, *window.shape))
        return -1, 0.0

    def _apply_nlp(self, result: RouteResult) -> None:
        """Run NLP grammar correction on the gesture ID."""
        if self._grammar_corrector is None or result.gesture_id < 0:
            return
        try:
            result.nlp_text = self._grammar_corrector([result.gesture_id])
        except Exception:
            logger.warning("NLP correction failed", exc_info=True)
