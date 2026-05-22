# -*- coding: utf-8 -*-
"""
TDD Tests for protobuf_parser — parse GloveData messages.

Pipeline: ESP32 serializes GloveData → UDP bytes → parse_glove_data() → dict
"""

import pytest
import struct
import time

from src.protobuf_parser import parse_glove_data, build_glove_data_dict, _HAS_PROTOBUF


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _make_protobuf_bytes(
    timestamp: int = 123456,
    hall: list[float] | None = None,
    imu: list[float] | None = None,
    flex: list[float] | None = None,
    l1_gesture_id: int = 0,
    l1_confidence: float = 0.0,
    l2_requested: bool = False,
    status: str = "STREAMING",
) -> bytes:
    """Build a valid GloveData protobuf message and return serialized bytes."""
    from proto import glove_data_pb2

    msg = glove_data_pb2.GloveData()
    msg.timestamp = timestamp
    if hall is not None:
        msg.hall_features.extend(hall)
    else:
        msg.hall_features.extend([0.0] * 15)
    if imu is not None:
        msg.imu_features.extend(imu)
    else:
        msg.imu_features.extend([0.0] * 6)
    if flex is not None:
        msg.flex_features.extend(flex)
    else:
        msg.flex_features.extend([0.0] * 5)
    msg.l1_gesture_id = l1_gesture_id
    msg.l1_confidence = l1_confidence
    msg.l2_requested = l2_requested
    msg.status = status
    return msg.SerializeToString()


# ---------------------------------------------------------------------------
# Test: Parse valid protobuf message
# ---------------------------------------------------------------------------

class TestParseValidProtobuf:
    """Parse well-formed GloveData protobuf messages."""

    def test_returns_dict(self):
        data = _make_protobuf_bytes()
        result = parse_glove_data(data)
        assert isinstance(result, dict)

    def test_timestamp_preserved(self):
        data = _make_protobuf_bytes(timestamp=999999)
        result = parse_glove_data(data)
        assert result["timestamp"] == 999999

    def test_hall_features_count(self):
        hall = [float(i) * 0.1 for i in range(15)]
        data = _make_protobuf_bytes(hall=hall)
        result = parse_glove_data(data)
        assert len(result["hall"]) == 15

    def test_hall_features_values(self):
        hall = [1.0, 2.0, 3.0] + [0.0] * 12
        data = _make_protobuf_bytes(hall=hall)
        result = parse_glove_data(data)
        assert result["hall"][0] == pytest.approx(1.0)
        assert result["hall"][1] == pytest.approx(2.0)
        assert result["hall"][2] == pytest.approx(3.0)

    def test_imu_features_count(self):
        imu = [0.1, -0.2, 0.3, 1.5, -2.0, 0.0]
        data = _make_protobuf_bytes(imu=imu)
        result = parse_glove_data(data)
        assert len(result["imu"]) == 6

    def test_imu_features_values(self):
        imu = [10.5, -20.3, 30.7, 1.1, 2.2, 3.3]
        data = _make_protobuf_bytes(imu=imu)
        result = parse_glove_data(data)
        assert result["imu"][0] == pytest.approx(10.5)
        assert result["imu"][1] == pytest.approx(-20.3)

    def test_flex_features_count(self):
        data = _make_protobuf_bytes()
        result = parse_glove_data(data)
        assert len(result["flex"]) == 5

    def test_l1_gesture_id(self):
        data = _make_protobuf_bytes(l1_gesture_id=7)
        result = parse_glove_data(data)
        assert result["l1_gesture_id"] == 7

    def test_l1_confidence(self):
        data = _make_protobuf_bytes(l1_confidence=0.95)
        result = parse_glove_data(data)
        assert result["l1_confidence"] == pytest.approx(0.95)

    def test_l2_requested_true(self):
        data = _make_protobuf_bytes(l2_requested=True)
        result = parse_glove_data(data)
        assert result["l2_requested"] is True

    def test_l2_requested_false(self):
        data = _make_protobuf_bytes(l2_requested=False)
        result = parse_glove_data(data)
        assert result["l2_requested"] is False

    def test_status_string(self):
        data = _make_protobuf_bytes(status="CALIBRATING")
        result = parse_glove_data(data)
        assert result["status"] == "CALIBRATING"


# ---------------------------------------------------------------------------
# Test: Parse invalid data
# ---------------------------------------------------------------------------

class TestParseInvalidData:
    """Error handling for malformed input."""

    def test_empty_bytes_returns_defaults(self):
        """Proto3: empty bytes = default message (all fields zero)."""
        result = parse_glove_data(b"")
        assert result["timestamp"] == 0
        assert result["l1_gesture_id"] == 0
        assert result["l1_confidence"] == pytest.approx(0.0)
        assert result["l2_requested"] is False

    def test_truncated_bytes_raises(self):
        with pytest.raises(ValueError):
            parse_glove_data(b"\x08\x99")

    def test_random_bytes_raises(self):
        import os
        with pytest.raises(ValueError):
            parse_glove_data(os.urandom(32))


# ---------------------------------------------------------------------------
# Test: Round-trip (serialize → parse)
# ---------------------------------------------------------------------------

class TestRoundTrip:
    """Serialize → parse preserves all fields."""

    def test_round_trip_all_fields(self):
        original = _make_protobuf_bytes(
            timestamp=42,
            hall=[1.1, 2.2, 3.3, 4.4, 5.5, 6.6, 7.7, 8.8, 9.9, 10.0, 11.1, 12.2, 13.3, 14.4, 15.5],
            imu=[0.1, -0.2, 0.3, 10.0, -20.0, 30.0],
            flex=[5.0, 4.0, 3.0, 2.0, 1.0],
            l1_gesture_id=12,
            l1_confidence=0.87,
            l2_requested=True,
            status="STREAMING",
        )
        result = parse_glove_data(original)
        assert result["timestamp"] == 42
        assert len(result["hall"]) == 15
        assert result["hall"][0] == pytest.approx(1.1)
        assert result["l1_gesture_id"] == 12
        assert result["l1_confidence"] == pytest.approx(0.87)
        assert result["l2_requested"] is True
        assert result["status"] == "STREAMING"

    def test_round_trip_zero_values(self):
        original = _make_protobuf_bytes(
            timestamp=0, l1_gesture_id=0, l1_confidence=0.0,
            l2_requested=False, status="IDLE",
        )
        result = parse_glove_data(original)
        assert result["timestamp"] == 0
        assert result["l1_gesture_id"] == 0
        assert result["l2_requested"] is False


# ---------------------------------------------------------------------------
# Test: build_glove_data_dict helper
# ---------------------------------------------------------------------------

class TestBuildGloveDataDict:
    """Test the dict builder helper."""

    def test_defaults(self):
        d = build_glove_data_dict()
        assert d["timestamp"] == 0
        assert len(d["hall"]) == 15
        assert len(d["imu"]) == 6
        assert len(d["flex"]) == 5
        assert d["l1_gesture_id"] == 0
        assert d["l1_confidence"] == 0.0
        assert d["l2_requested"] is False
        assert d["status"] == "IDLE"

    def test_custom_values(self):
        d = build_glove_data_dict(
            timestamp=100,
            hall=[1.0] * 15,
            l1_gesture_id=5,
            l1_confidence=0.9,
            l2_requested=True,
            status="STREAMING",
        )
        assert d["timestamp"] == 100
        assert d["l1_gesture_id"] == 5
        assert d["l2_requested"] is True
