"""Tests for V5 protobuf schema."""
import pytest
from proto.glove_data_pb2 import GloveData, ReceiverPacket


def test_glove_data_roundtrip():
    gd = GloveData()
    gd.version = 5
    gd.timestamp = 1234567
    gd.hand_id = 0
    gd.flex.extend([0.1, 0.2, 0.3, 0.4, 0.5])
    gd.imu.extend([10.0, 20.0, 30.0, 1.0, 2.0, 3.0])
    gd.l1_gesture_id = 15
    gd.l1_confidence = 0.95
    gd.status = "streaming"
    data = gd.SerializeToString()
    gd2 = GloveData()
    gd2.ParseFromString(data)
    assert gd2.version == 5
    assert gd2.hand_id == 0
    assert len(gd2.flex) == 5
    assert len(gd2.imu) == 6
    assert gd2.l1_gesture_id == 15
    assert abs(gd2.l1_confidence - 0.95) < 1e-6


def test_receiver_packet():
    rp = ReceiverPacket()
    rp.version = 5
    rp.tick_id = 42
    rp.left.version = 5
    rp.left.hand_id = 0
    rp.left.flex.extend([0.1] * 5)
    rp.left.imu.extend([0.0] * 6)
    rp.right.version = 5
    rp.right.hand_id = 1
    rp.right.flex.extend([0.2] * 5)
    rp.right.imu.extend([0.0] * 6)
    rp.relative_features.extend([0.1, 0.2, 0.3, 0.01, 0.5, 0.33])
    data = rp.SerializeToString()
    rp2 = ReceiverPacket()
    rp2.ParseFromString(data)
    assert rp2.tick_id == 42
    assert len(rp2.relative_features) == 6
    assert rp2.left.hand_id == 0
    assert rp2.right.hand_id == 1


def test_relative_features_placeholder():
    gd = GloveData()
    gd.relative.extend([0.0] * 6)
    assert len(gd.relative) == 6
