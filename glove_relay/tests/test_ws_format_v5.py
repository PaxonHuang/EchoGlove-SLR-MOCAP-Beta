"""Tests for V5 WebSocket JSON format."""
import pytest
import json


def test_ws_json_dual_hand_format():
    msg = {
        "tier": "tier2",
        "blend_alpha": 1.0,
        "left_hand": {
            "flex": [0.1, 0.2, 0.3, 0.4, 0.5],
            "euler": [10.0, 20.0, 30.0],
            "gyro": [1.0, 2.0, 3.0],
            "gesture_id": 5,
            "confidence": 0.9,
        },
        "right_hand": {
            "flex": [0.6, 0.7, 0.8, 0.9, 1.0],
            "euler": [40.0, 50.0, 60.0],
            "gyro": [4.0, 5.0, 6.0],
            "gesture_id": 10,
            "confidence": 0.85,
        },
        "relative": {
            "delta_euler": [0.1, 0.2, 0.3],
            "delta_quat_dist": 0.01,
            "delta_gyro_norm": 0.5,
            "delta_gyro_axis": 0.33,
        },
        "inference": {
            "gesture_id": 8,
            "confidence": 0.95,
            "text": "hello",
        },
        "nlp_text": "你好",
    }
    serialized = json.dumps(msg)
    parsed = json.loads(serialized)
    assert parsed["tier"] == "tier2"
    assert len(parsed["left_hand"]["flex"]) == 5
    assert len(parsed["right_hand"]["flex"]) == 5
    assert len(parsed["relative"]["delta_euler"]) == 3
    assert "nlp_text" in parsed
