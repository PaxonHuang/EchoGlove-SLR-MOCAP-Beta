# -*- coding: utf-8 -*-
"""
TDD Tests for UDPServer — datagram handling, inference pipeline, broadcast.

Tests use mocks to avoid real sockets and model dependencies.
"""

import json
import time
import pytest
from unittest.mock import AsyncMock, MagicMock, patch

from src.udp_server import UDPServer
from src.protobuf_parser import build_glove_data_dict


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _make_server(**kwargs) -> UDPServer:
    """Create a UDPServer with a mock callback."""
    cb = kwargs.pop("on_data_callback", MagicMock())
    return UDPServer(
        host=kwargs.get("host", "0.0.0.0"),
        port=kwargs.get("port", 8888),
        buffer_size=kwargs.get("buffer_size", 4096),
        on_data_callback=cb,
    )


# ---------------------------------------------------------------------------
# Test: Construction
# ---------------------------------------------------------------------------

class TestUDPServerConstruction:
    """Server initialisation and defaults."""

    def test_default_host_port(self):
        srv = UDPServer("0.0.0.0", 8888)
        assert srv.host == "0.0.0.0"
        assert srv.port == 8888

    def test_running_flag_true(self):
        srv = UDPServer("0.0.0.0", 8888)
        assert srv.running is True

    def test_custom_buffer_size(self):
        srv = UDPServer("0.0.0.0", 8888, buffer_size=2048)
        assert srv.buffer_size == 2048

    def test_callback_stored(self):
        cb = MagicMock()
        srv = UDPServer("0.0.0.0", 8888, on_data_callback=cb)
        assert srv.on_data_callback is cb

    def test_no_callback_allowed(self):
        srv = UDPServer("0.0.0.0", 8888)
        assert srv.on_data_callback is None


# ---------------------------------------------------------------------------
# Test: _handle_datagram — valid data
# ---------------------------------------------------------------------------

class TestHandleDatagramValid:
    """Processing well-formed protobuf datagrams."""

    def test_callback_invoked(self):
        cb = MagicMock()
        srv = _make_server(on_data_callback=cb)
        data = build_glove_data_dict(timestamp=100)
        from proto import glove_data_pb2
        msg = glove_data_pb2.GloveData()
        msg.timestamp = 100
        msg.hall_features.extend([0.0] * 15)
        msg.imu_features.extend([0.0] * 6)
        msg.flex_features.extend([0.0] * 5)
        raw = msg.SerializeToString()

        with patch.object(srv, "_run_l1", return_value=(3, 0.92)):
            srv._handle_datagram(raw, ("192.168.1.10", 5000))

        cb.assert_called_once()

    def test_result_has_required_keys(self):
        cb = MagicMock()
        srv = _make_server(on_data_callback=cb)
        from proto import glove_data_pb2
        msg = glove_data_pb2.GloveData()
        msg.timestamp = 42
        msg.hall_features.extend([1.0] * 15)
        msg.imu_features.extend([2.0] * 6)
        msg.flex_features.extend([0.0] * 5)
        raw = msg.SerializeToString()

        with patch.object(srv, "_run_l1", return_value=(5, 0.88)):
            srv._handle_datagram(raw, ("10.0.0.1", 9999))

        result = cb.call_args[0][0]
        expected_keys = {"timestamp", "hall", "imu", "l1_gesture_id", "l1_confidence",
                         "l2_gesture_id", "l2_confidence", "nlp_text", "status"}
        assert expected_keys.issubset(result.keys())

    def test_l1_result_propagated(self):
        cb = MagicMock()
        srv = _make_server(on_data_callback=cb)
        from proto import glove_data_pb2
        msg = glove_data_pb2.GloveData()
        msg.timestamp = 1
        raw = msg.SerializeToString()

        with patch.object(srv, "_run_l1", return_value=(7, 0.95)):
            srv._handle_datagram(raw, ("127.0.0.1", 8888))

        result = cb.call_args[0][0]
        assert result["l1_gesture_id"] == 7
        assert result["l1_confidence"] == pytest.approx(0.95)

    def test_hall_data_in_result(self):
        cb = MagicMock()
        srv = _make_server(on_data_callback=cb)
        from proto import glove_data_pb2
        msg = glove_data_pb2.GloveData()
        msg.timestamp = 1
        msg.hall_features.extend([1.1, 2.2, 3.3] + [0.0] * 12)
        msg.imu_features.extend([0.0] * 6)
        msg.flex_features.extend([0.0] * 5)
        raw = msg.SerializeToString()

        with patch.object(srv, "_run_l1", return_value=(0, 0.5)):
            srv._handle_datagram(raw, ("127.0.0.1", 8888))

        result = cb.call_args[0][0]
        assert len(result["hall"]) == 15
        assert result["hall"][0] == pytest.approx(1.1)

    def test_default_l2_values(self):
        """Without L2 trigger, result has placeholder L2 values."""
        cb = MagicMock()
        srv = _make_server(on_data_callback=cb)
        from proto import glove_data_pb2
        msg = glove_data_pb2.GloveData()
        msg.timestamp = 1
        raw = msg.SerializeToString()

        with patch.object(srv, "_run_l1", return_value=(0, 0.5)):
            srv._handle_datagram(raw, ("127.0.0.1", 8888))

        result = cb.call_args[0][0]
        assert result["l2_gesture_id"] == -1
        assert result["l2_confidence"] == pytest.approx(0.0)


# ---------------------------------------------------------------------------
# Test: _handle_datagram — invalid data
# ---------------------------------------------------------------------------

class TestHandleDatagramInvalid:
    """Graceful handling of bad datagrams."""

    def test_garbage_bytes_no_callback(self):
        cb = MagicMock()
        srv = _make_server(on_data_callback=cb)
        srv._handle_datagram(b"\xff\xfe\xfd\xfc", ("127.0.0.1", 8888))
        cb.assert_not_called()

    def test_empty_bytes_no_callback(self):
        cb = MagicMock()
        srv = _make_server(on_data_callback=cb)
        # Empty bytes parse as default protobuf (valid), so callback IS called
        with patch.object(srv, "_run_l1", return_value=(-1, 0.0)):
            srv._handle_datagram(b"", ("127.0.0.1", 8888))
        # Default proto is valid — should still invoke callback
        cb.assert_called_once()

    def test_no_callback_no_crash(self):
        srv = _make_server(on_data_callback=None)
        from proto import glove_data_pb2
        msg = glove_data_pb2.GloveData()
        msg.timestamp = 1
        raw = msg.SerializeToString()
        # Should not raise
        with patch.object(srv, "_run_l1", return_value=(-1, 0.0)):
            srv._handle_datagram(raw, ("127.0.0.1", 8888))


# ---------------------------------------------------------------------------
# Test: Callback exception handling
# ---------------------------------------------------------------------------

class TestCallbackException:
    """Broadcast callback errors should not crash the server."""

    def test_callback_exception_swallowed(self):
        cb = MagicMock(side_effect=RuntimeError("ws closed"))
        srv = _make_server(on_data_callback=cb)
        from proto import glove_data_pb2
        msg = glove_data_pb2.GloveData()
        msg.timestamp = 1
        raw = msg.SerializeToString()

        with patch.object(srv, "_run_l1", return_value=(0, 0.5)):
            # Should not raise
            srv._handle_datagram(raw, ("127.0.0.1", 8888))


# ---------------------------------------------------------------------------
# Test: L1 → L2 routing logic (SOP §9.3)
# ---------------------------------------------------------------------------

def _make_protobuf_raw(timestamp: int = 1) -> bytes:
    """Helper to build a valid protobuf datagram."""
    from proto import glove_data_pb2
    msg = glove_data_pb2.GloveData()
    msg.timestamp = timestamp
    msg.hall_features.extend([0.5] * 15)
    msg.imu_features.extend([0.1] * 6)
    msg.flex_features.extend([0.0] * 5)
    return msg.SerializeToString()


class TestL2Routing:
    """Confidence-driven L1 → L2 gating per SOP §9.3."""

    def test_high_confidence_skips_l2(self):
        """L1 confidence > 0.85 → L2 not triggered, status l1_ok."""
        cb = MagicMock()
        srv = _make_server(on_data_callback=cb)
        raw = _make_protobuf_raw()

        with patch.object(srv, "_run_l1", return_value=(5, 0.95)):
            srv._handle_datagram(raw, ("127.0.0.1", 8888))

        result = cb.call_args[0][0]
        assert result["status"] == "l1_ok"
        assert result["l2_gesture_id"] == -1

    def test_low_confidence_triggers_l2(self):
        """L1 confidence <= 0.85 with debounce satisfied and window full → L2 fires."""
        cb = MagicMock()
        srv = _make_server(on_data_callback=cb)
        srv._window_size = 3
        srv._debounce_frames = 1  # 1 debounce frame, then buffer starts
        srv._last_gesture_time = 0.0
        raw = _make_protobuf_raw()

        with patch.object(srv, "_run_l1", return_value=(3, 0.50)):
            with patch.object(srv, "_run_l2", return_value=(12, 0.91)):
                # 1 debounce + (window_size+1 due to pre-append check) = 6 frames
                for _ in range(6):
                    srv._handle_datagram(raw, ("127.0.0.1", 8888))

        # L2 fires on frame 5 (index 5)
        l2_call = cb.call_args_list[5]
        result = l2_call[0][0]
        assert result["l2_gesture_id"] == 12
        assert result["l2_confidence"] == pytest.approx(0.91)
        assert result["status"] == "l2_ok"

    def test_debounce_counter_increments(self):
        """Debounce counter increments on each low-confidence frame."""
        srv = _make_server(on_data_callback=MagicMock())
        raw = _make_protobuf_raw()

        assert srv._debounce_counter == 0
        with patch.object(srv, "_run_l1", return_value=(0, 0.50)):
            srv._handle_datagram(raw, ("127.0.0.1", 8888))
        assert srv._debounce_counter == 1

        with patch.object(srv, "_run_l1", return_value=(0, 0.50)):
            srv._handle_datagram(raw, ("127.0.0.1", 8888))
        assert srv._debounce_counter == 2

    def test_debounce_not_satisfied_no_l2(self):
        """Before debounce_frames reached, L2 should not trigger even if low confidence."""
        cb = MagicMock()
        srv = _make_server(on_data_callback=cb)
        srv._debounce_frames = 3
        raw = _make_protobuf_raw()
        srv._last_gesture_time = 0.0  # past silence

        with patch.object(srv, "_run_l1", return_value=(0, 0.30)):
            # Send 2 frames (need 3 for debounce)
            srv._handle_datagram(raw, ("127.0.0.1", 8888))
            srv._handle_datagram(raw, ("127.0.0.1", 8888))

        # Neither frame should trigger L2
        for call in cb.call_args_list:
            result = call[0][0]
            assert result["l2_gesture_id"] == -1

    def test_frame_buffer_accumulates(self):
        """Low-confidence frames accumulate in the sliding window buffer after debounce."""
        srv = _make_server(on_data_callback=MagicMock())
        srv._debounce_frames = 0  # disable debounce to isolate buffer behavior
        raw = _make_protobuf_raw()
        srv._last_gesture_time = 0.0

        with patch.object(srv, "_run_l1", return_value=(0, 0.40)):
            for _ in range(5):
                srv._handle_datagram(raw, ("127.0.0.1", 8888))

        assert len(srv._frame_buffer) == 5

    def test_l2_triggers_at_window_size(self):
        """L2 fires when buffer reaches l2_window_size (check is pre-append)."""
        cb = MagicMock()
        srv = _make_server(on_data_callback=cb)
        srv._window_size = 5
        srv._debounce_frames = 0
        srv._last_gesture_time = 0.0
        raw = _make_protobuf_raw()

        with patch.object(srv, "_run_l1", return_value=(0, 0.40)):
            with patch.object(srv, "_run_l2", return_value=(22, 0.88)):
                for _ in range(5):
                    srv._handle_datagram(raw, ("127.0.0.1", 8888))

        # L2 fires on the 5th frame (index 4) when buffer reaches window_size
        l2_call = cb.call_args_list[4]
        result = l2_call[0][0]
        assert result["l2_gesture_id"] == 22
        assert result["status"] == "l2_ok"

    def test_l2_clears_buffer_after_trigger(self):
        """After L2 fires, frame_buffer is cleared."""
        srv = _make_server(on_data_callback=MagicMock())
        srv._window_size = 3
        srv._debounce_frames = 0
        srv._last_gesture_time = 0.0
        raw = _make_protobuf_raw()

        with patch.object(srv, "_run_l1", return_value=(0, 0.40)):
            with patch.object(srv, "_run_l2", return_value=(10, 0.90)):
                # window_size + 1 to trigger, buffer clears after
                for _ in range(4):
                    srv._handle_datagram(raw, ("127.0.0.1", 8888))

        assert len(srv._frame_buffer) == 0

    def test_l2_disabled_skips_l2(self):
        """When l2_enabled=False, L2 never triggers."""
        cb = MagicMock()
        srv = _make_server(on_data_callback=cb)
        srv._config.inference.l2_enabled = False
        srv._debounce_frames = 0
        srv._last_gesture_time = 0.0
        raw = _make_protobuf_raw()

        with patch.object(srv, "_run_l1", return_value=(0, 0.30)):
            for _ in range(35):
                srv._handle_datagram(raw, ("127.0.0.1", 8888))

        # Even after many frames, L2 should not have fired
        for call in cb.call_args_list:
            result = call[0][0]
            assert result["l2_gesture_id"] == -1

    def test_silence_period_prevents_immediate_retrigger(self):
        """After L2 fires, silence_ms prevents immediate re-trigger."""
        cb = MagicMock()
        srv = _make_server(on_data_callback=cb)
        srv._window_size = 2
        srv._debounce_frames = 0
        srv._silence_ms = 500
        raw = _make_protobuf_raw()

        with patch.object(srv, "_run_l1", return_value=(0, 0.40)):
            with patch.object(srv, "_run_l2", return_value=(10, 0.90)):
                # First batch: triggers L2 (last_gesture_time = 0, well past silence)
                srv._last_gesture_time = 0.0
                srv._handle_datagram(raw, ("127.0.0.1", 8888))
                srv._handle_datagram(raw, ("127.0.0.1", 8888))

        # After trigger, _last_gesture_time is set to now
        # Second batch immediately: should NOT trigger L2 (within silence period)
        cb.reset_mock()
        with patch.object(srv, "_run_l1", return_value=(0, 0.40)):
            srv._handle_datagram(raw, ("127.0.0.1", 8888))
            srv._handle_datagram(raw, ("127.0.0.1", 8888))

        # L2 should not fire — silence period not elapsed
        for call in cb.call_args_list:
            result = call[0][0]
            assert result["l2_gesture_id"] == -1
