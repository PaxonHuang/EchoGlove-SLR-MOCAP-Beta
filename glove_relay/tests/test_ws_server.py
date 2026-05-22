# -*- coding: utf-8 -*-
"""
TDD Tests for ws_server.ConnectionManager.

Tests WebSocket connection lifecycle and broadcast behavior using mocks
(FastAPI WebSocket objects are not directly instantiable).
"""

import pytest
import asyncio
from unittest.mock import AsyncMock, MagicMock, patch

from src.ws_server import ConnectionManager


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _make_mock_ws() -> AsyncMock:
    """Create a mock FastAPI WebSocket."""
    ws = AsyncMock()
    ws.accept = AsyncMock()
    ws.send_text = AsyncMock()
    ws.close = AsyncMock()
    ws.receive_text = AsyncMock(return_value="ping")
    return ws


# ---------------------------------------------------------------------------
# Test: Connection lifecycle
# ---------------------------------------------------------------------------

class TestConnectionLifecycle:
    """connect / disconnect / active_count."""

    @pytest.mark.asyncio
    async def test_initial_count_zero(self):
        mgr = ConnectionManager()
        assert mgr.active_count == 0

    @pytest.mark.asyncio
    async def test_connect_increments_count(self):
        mgr = ConnectionManager()
        ws = _make_mock_ws()
        await mgr.connect(ws)
        assert mgr.active_count == 1

    @pytest.mark.asyncio
    async def test_connect_calls_accept(self):
        mgr = ConnectionManager()
        ws = _make_mock_ws()
        await mgr.connect(ws)
        ws.accept.assert_called_once()

    @pytest.mark.asyncio
    async def test_disconnect_decrements_count(self):
        mgr = ConnectionManager()
        ws = _make_mock_ws()
        await mgr.connect(ws)
        mgr.disconnect(ws)
        assert mgr.active_count == 0

    @pytest.mark.asyncio
    async def test_disconnect_unknown_ws_no_error(self):
        mgr = ConnectionManager()
        ws = _make_mock_ws()
        mgr.disconnect(ws)  # Should not raise
        assert mgr.active_count == 0

    @pytest.mark.asyncio
    async def test_multiple_connections(self):
        mgr = ConnectionManager()
        for _ in range(5):
            await mgr.connect(_make_mock_ws())
        assert mgr.active_count == 5

    @pytest.mark.asyncio
    async def test_disconnect_one_of_many(self):
        mgr = ConnectionManager()
        ws1 = _make_mock_ws()
        ws2 = _make_mock_ws()
        await mgr.connect(ws1)
        await mgr.connect(ws2)
        mgr.disconnect(ws1)
        assert mgr.active_count == 1


# ---------------------------------------------------------------------------
# Test: Broadcast
# ---------------------------------------------------------------------------

class TestBroadcast:
    """data broadcast to all connected clients."""

    @pytest.mark.asyncio
    async def test_broadcast_sends_to_all(self):
        mgr = ConnectionManager()
        ws1 = _make_mock_ws()
        ws2 = _make_mock_ws()
        await mgr.connect(ws1)
        await mgr.connect(ws2)

        await mgr.broadcast({"type": "sensor_data", "value": 42})

        ws1.send_text.assert_called_once()
        ws2.send_text.assert_called_once()

    @pytest.mark.asyncio
    async def test_broadcast_sends_json(self):
        mgr = ConnectionManager()
        ws = _make_mock_ws()
        await mgr.connect(ws)

        await mgr.broadcast({"gesture": "hello", "confidence": 0.95})

        import json
        call_args = ws.send_text.call_args[0][0]
        parsed = json.loads(call_args)
        assert parsed["gesture"] == "hello"
        assert parsed["confidence"] == pytest.approx(0.95)

    @pytest.mark.asyncio
    async def test_broadcast_empty_noop(self):
        mgr = ConnectionManager()
        # No connections — should not raise
        await mgr.broadcast({"data": 1})

    @pytest.mark.asyncio
    async def test_broadcast_dead_client_removed(self):
        mgr = ConnectionManager()
        ws_good = _make_mock_ws()
        ws_dead = _make_mock_ws()
        ws_dead.send_text = AsyncMock(side_effect=Exception("connection closed"))

        await mgr.connect(ws_good)
        await mgr.connect(ws_dead)

        await mgr.broadcast({"data": "test"})

        # Dead client should be removed
        assert mgr.active_count == 1

    @pytest.mark.asyncio
    async def test_broadcast_unicode_data(self):
        mgr = ConnectionManager()
        ws = _make_mock_ws()
        await mgr.connect(ws)

        await mgr.broadcast({"nlp_text": "你好世界"})

        import json
        call_args = ws.send_text.call_args[0][0]
        parsed = json.loads(call_args)
        assert parsed["nlp_text"] == "你好世界"


# ---------------------------------------------------------------------------
# Test: Cleanup
# ---------------------------------------------------------------------------

class TestCleanup:
    """close_all graceful shutdown."""

    @pytest.mark.asyncio
    async def test_close_all_clears_connections(self):
        mgr = ConnectionManager()
        for _ in range(3):
            await mgr.connect(_make_mock_ws())

        await mgr.close_all()
        assert mgr.active_count == 0

    @pytest.mark.asyncio
    async def test_close_all_calls_close(self):
        mgr = ConnectionManager()
        ws = _make_mock_ws()
        await mgr.connect(ws)

        await mgr.close_all()
        ws.close.assert_called_once()

    @pytest.mark.asyncio
    async def test_close_all_handles_already_closed(self):
        mgr = ConnectionManager()
        ws = _make_mock_ws()
        ws.close = AsyncMock(side_effect=Exception("already closed"))
        await mgr.connect(ws)

        # Should not raise
        await mgr.close_all()
        assert mgr.active_count == 0
