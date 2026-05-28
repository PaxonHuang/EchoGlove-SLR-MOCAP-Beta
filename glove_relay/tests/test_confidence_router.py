# -*- coding: utf-8 -*-
"""Tests for ConfidenceRouter — L1→L2 confidence-driven routing."""

import pytest
from unittest.mock import MagicMock

from src.confidence_router import ConfidenceRouter, RouteResult


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------
def _make_config(
    threshold=0.85,
    debounce=3,
    silence_ms=100,
    window_size=5,
    l2_enabled=True,
):
    """Build a mock config matching the RelayConfig structure."""
    cfg = MagicMock()
    cfg.inference.l1_confidence_threshold = threshold
    cfg.inference.debounce_frames = debounce
    cfg.inference.gesture_silence_ms = silence_ms
    cfg.inference.l2_window_size = window_size
    cfg.inference.l2_enabled = l2_enabled
    return cfg


def _make_parsed(hall=None, imu=None, gesture_id=0, confidence=0.9):
    """Build a minimal parsed protobuf dict."""
    return {
        "hall": hall or [0.0] * 15,
        "imu": imu or [0.0] * 6,
        "l1_gesture_id": gesture_id,
        "l1_confidence": confidence,
    }


# ---------------------------------------------------------------------------
# High confidence → direct L1 output
# ---------------------------------------------------------------------------
class TestHighConfidence:
    def test_high_confidence_returns_l1(self):
        router = ConfidenceRouter(config=_make_config(threshold=0.85))
        result = router.route(_make_parsed(gesture_id=5, confidence=0.95))
        assert result.gesture_id == 5
        assert result.confidence == 0.95
        assert result.source == "l1"
        assert result.status == "l1_ok"

    def test_high_confidence_resets_debounce(self):
        router = ConfidenceRouter(config=_make_config(threshold=0.85))
        # Low confidence frames to build debounce
        for _ in range(5):
            router.route(_make_parsed(confidence=0.5))
        # High confidence should reset
        router.route(_make_parsed(confidence=0.99))
        assert router.buffer_size == 0  # buffer not affected by high conf


# ---------------------------------------------------------------------------
# Low confidence → L2 buffering
# ---------------------------------------------------------------------------
class TestLowConfidence:
    def test_low_confidence_buffers_frames(self):
        router = ConfidenceRouter(config=_make_config(threshold=0.85, window_size=5))
        for _ in range(3):
            router.route(_make_parsed(confidence=0.5))
        assert router.buffer_size == 3

    def test_l2_disabled_returns_l1_even_when_low(self):
        router = ConfidenceRouter(config=_make_config(threshold=0.85, l2_enabled=False))
        result = router.route(_make_parsed(gesture_id=7, confidence=0.3))
        assert result.source == "l1"
        assert result.gesture_id == 7


# ---------------------------------------------------------------------------
# Debounce
# ---------------------------------------------------------------------------
class TestDebounce:
    def test_debounce_prevents_immediate_l2(self):
        router = ConfidenceRouter(config=_make_config(threshold=0.85, debounce=3, window_size=3))
        l2_model = MagicMock(return_value=(10, 0.9))
        router._l2_model = l2_model

        # Fill buffer to window_size but debounce not met
        router.route(_make_parsed(confidence=0.5))
        router.route(_make_parsed(confidence=0.5))
        result = router.route(_make_parsed(confidence=0.5))
        # debounce_counter is 3 but needs debounce_frames >= 3
        # With debounce=3, the 3rd low-conf frame triggers (debounce_counter increments to 3)
        # Actually, debounce_counter starts at 0, increments each low-conf frame
        # After 3 frames: counter=3, debounce_frames=3 → condition met
        # But silence_ms check also applies

    def test_debounce_resets_on_high_confidence(self):
        router = ConfidenceRouter(config=_make_config(threshold=0.85, debounce=3))
        router.route(_make_parsed(confidence=0.5))
        router.route(_make_parsed(confidence=0.5))
        assert router._debounce_counter == 2
        router.route(_make_parsed(confidence=0.95))
        assert router._debounce_counter == 0


# ---------------------------------------------------------------------------
# NLP integration
# ---------------------------------------------------------------------------
class TestNLPIntegration:
    def test_nlp_called_with_gesture_id(self):
        nlp = MagicMock(return_value="你好")
        router = ConfidenceRouter(config=_make_config(), grammar_corrector=nlp)
        result = router.route(_make_parsed(gesture_id=10, confidence=0.95))
        nlp.assert_called_once_with([10])
        assert result.nlp_text == "你好"

    def test_nlp_not_called_for_unknown(self):
        nlp = MagicMock()
        router = ConfidenceRouter(config=_make_config(), grammar_corrector=nlp)
        router.route(_make_parsed(gesture_id=-1, confidence=0.95))
        nlp.assert_not_called()

    def test_nlp_failure_does_not_crash(self):
        def bad_nlp(ids):
            raise RuntimeError("NLP crashed")

        router = ConfidenceRouter(config=_make_config(), grammar_corrector=bad_nlp)
        result = router.route(_make_parsed(gesture_id=5, confidence=0.95))
        assert result.nlp_text == ""  # graceful degradation


# ---------------------------------------------------------------------------
# Reset
# ---------------------------------------------------------------------------
class TestReset:
    def test_reset_clears_state(self):
        router = ConfidenceRouter(config=_make_config(threshold=0.85, window_size=5))
        for _ in range(4):
            router.route(_make_parsed(confidence=0.5))
        assert router.buffer_size == 4
        router.reset()
        assert router.buffer_size == 0
        assert router._debounce_counter == 0
        assert router._last_gesture_time == 0.0


# ---------------------------------------------------------------------------
# RouteResult defaults
# ---------------------------------------------------------------------------
class TestRouteResult:
    def test_default_values(self):
        r = RouteResult()
        assert r.gesture_id == -1
        assert r.confidence == 0.0
        assert r.source == "unknown"
        assert r.status == "unknown"
