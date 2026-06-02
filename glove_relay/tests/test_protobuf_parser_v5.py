"""Tests for V5 protobuf parser."""
import pytest
from src.protobuf_parser import ProtobufParser
from proto.glove_data_pb2 import ReceiverPacket


@pytest.fixture
def parser():
    return ProtobufParser()


def _make_receiver_packet(tick_id=1, left_flex=None, right_flex=None):
    rp = ReceiverPacket()
    rp.version = 5
    rp.tick_id = tick_id
    rp.left.version = 5
    rp.left.hand_id = 0
    rp.left.flex.extend(left_flex or [0.1, 0.2, 0.3, 0.4, 0.5])
    rp.left.imu.extend([10.0, 20.0, 30.0, 1.0, 2.0, 3.0])
    rp.left.l1_gesture_id = 5
    rp.left.l1_confidence = 0.9
    rp.right.version = 5
    rp.right.hand_id = 1
    rp.right.flex.extend(right_flex or [0.6, 0.7, 0.8, 0.9, 1.0])
    rp.right.imu.extend([40.0, 50.0, 60.0, 4.0, 5.0, 6.0])
    rp.right.l1_gesture_id = 10
    rp.right.l1_confidence = 0.85
    rp.relative_features.extend([0.1, 0.2, 0.3, 0.01, 0.5, 0.33])
    return rp


def test_parse_v5_receiver_packet(parser):
    rp = _make_receiver_packet()
    data = rp.SerializeToString()
    result = parser.parse_receiver_packet(data)
    assert result is not None
    assert result["tick_id"] == 1
    assert result["left"]["hand_id"] == 0
    assert result["right"]["hand_id"] == 1
    assert len(result["left"]["flex"]) == 5
    assert len(result["relative_features"]) == 6


def test_parse_v5_version_check(parser):
    rp = _make_receiver_packet()
    rp.version = 3
    data = rp.SerializeToString()
    result = parser.parse_receiver_packet(data)
    assert result is None


def test_parse_v5_28dim_features(parser):
    rp = _make_receiver_packet()
    data = rp.SerializeToString()
    result = parser.parse_receiver_packet(data)
    features = parser.assemble_28dim(result)
    assert len(features) == 28
    assert abs(features[0] - 0.1) < 1e-6   # left flex[0]
    assert abs(features[11] - 0.6) < 1e-6  # right flex[0]
    assert abs(features[22] - 0.1) < 1e-6  # relative[0]
