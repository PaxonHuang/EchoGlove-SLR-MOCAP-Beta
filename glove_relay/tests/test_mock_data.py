# -*- coding: utf-8 -*-
"""Tests for the mock data source."""

from __future__ import annotations

import asyncio
import json
from typing import Any

import pytest

from src.mock_data import MockDataSource


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------
class _BroadcastCollector:
    """Coroutine callable that collects broadcast payloads."""

    def __init__(self) -> None:
        self.messages: list[dict[str, Any]] = []

    async def __call__(self, data: dict[str, Any]) -> None:
        self.messages.append(data)


# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
class TestMockDataSource:
    """Unit tests for MockDataSource message generation and pacing."""

    def test_message_schema_valid(self) -> None:
        src = MockDataSource(seed=42)
        msg = src._build_message(elapsed=0.0, frame=0)

        # Top-level keys match V5 schema
        assert "tier" in msg
        assert "blend_alpha" in msg
        assert "left_hand" in msg
        assert "right_hand" in msg
        assert "relative" in msg
        assert "inference" in msg
        assert "nlp_text" in msg

        # Hand data shapes
        for hand in (msg["left_hand"], msg["right_hand"]):
            assert len(hand["flex"]) == 5
            assert len(hand["euler"]) == 3
            assert len(hand["gyro"]) == 3
            assert 0.0 <= hand["confidence"] <= 1.0

        # Relative/inference shapes
        assert len(msg["relative"]["delta_euler"]) == 3
        assert isinstance(msg["inference"]["text"], str)
        assert isinstance(msg["nlp_text"], str)

    def test_flex_values_in_range(self) -> None:
        src = MockDataSource(seed=123)
        for frame in range(100):
            msg = src._build_message(elapsed=frame * 0.05, frame=frame)
            for hand in (msg["left_hand"], msg["right_hand"]):
                for v in hand["flex"]:
                    assert 0.0 <= v <= 1.0

    def test_gesture_cycles(self) -> None:
        src = MockDataSource(gesture_cycle_seconds=1.0, seed=0)
        # Just before first cycle ends
        msg_a = src._build_message(elapsed=0.9, frame=0)
        # Just after first cycle ends
        msg_b = src._build_message(elapsed=1.1, frame=0)
        # Different gesture index expected
        assert msg_a["left_hand"]["gesture_id"] != msg_b["left_hand"]["gesture_id"]

    @pytest.mark.asyncio
    async def test_pacing_approximate(self) -> None:
        fps = 20.0
        src = MockDataSource(fps=fps, seed=0)
        collector = _BroadcastCollector()

        task = src.start(collector)
        await asyncio.sleep(0.55)  # expect ~11 messages at 20 fps
        await src.stop()

        # Tolerate scheduling jitter
        assert 8 <= len(collector.messages) <= 14

        # Verify consecutive messages are different (noise + interpolation)
        assert collector.messages[0] != collector.messages[1]

    @pytest.mark.asyncio
    async def test_stop_cancels_task(self) -> None:
        src = MockDataSource(seed=0)
        collector = _BroadcastCollector()

        task = src.start(collector)
        assert not task.done()
        await src.stop()
        assert task.done()
        assert src._task is None

    @pytest.mark.asyncio
    async def test_json_serializable(self) -> None:
        src = MockDataSource(seed=0)
        msg = src._build_message(elapsed=0.0, frame=0)
        # Should not raise
        json_str = json.dumps(msg, ensure_ascii=False)
        assert isinstance(json_str, str)
        parsed = json.loads(json_str)
        assert parsed["tier"] == "tier2"
