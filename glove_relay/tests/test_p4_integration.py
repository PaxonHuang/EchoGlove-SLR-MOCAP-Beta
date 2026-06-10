"""Integration test for P4 base station data pipeline."""
from __future__ import annotations
import pytest
from src.protobuf_parser import ProtobufParser
from src.confidence_router import ConfidenceRouter
from proto.glove_data_pb2 import BaseStationPacket


class TestP4Integration:
    def setup_method(self):
        self.parser = ProtobufParser()
        self.router = ConfidenceRouter()

    def _make_base_station_packet(self, gesture_id: int = 0,
                                   confidence: float = 0.0) -> bytes:
        bsp = BaseStationPacket()
        bsp.version = 5
        bsp.tick_id = 42
        bsp.left.flex.extend([0.1, 0.2, 0.3, 0.4, 0.5])
        bsp.left.imu.extend([1.0, 2.0, 3.0, 0.1, 0.2, 0.3])
        bsp.right.flex.extend([0.6, 0.7, 0.8, 0.9, 1.0])
        bsp.right.imu.extend([4.0, 5.0, 6.0, 0.4, 0.5, 0.6])
        bsp.relative_features.extend([0.1, 0.2, 0.3, 0.4, 0.5, 0.6])
        bsp.tier2_gesture_id = gesture_id
        bsp.tier2_confidence = confidence
        bsp.base_station.c6_connected = True
        bsp.base_station.p4_ready = True
        bsp.base_station.active_tier = 2
        return bsp.SerializeToString()

    def test_full_pipeline_parse_assemble_route(self):
        """Verify parse -> 28-dim assembly -> confidence routing."""
        data = self._make_base_station_packet(gesture_id=5, confidence=0.92)
        parsed = self.parser.parse_base_station_packet(data)
        assert parsed is not None

        features = self.parser.assemble_28dim(parsed)
        assert len(features) == 28

        # Route through confidence router
        tier2_result = {
            "gesture_id": parsed["tier2_gesture_id"],
            "confidence": parsed["tier2_confidence"],
        }
        routed = self.router.route(tier2=tier2_result)
        assert routed["gesture_id"] == 5
        assert routed["active_tier"] == "tier2"

    def test_hot_switch_tier1_to_tier2(self):
        """Verify router switches from tier1 to tier2 when P4 sends results."""
        # Tier1 only
        r1 = self.router.route(tier1={"gesture_id": 1, "confidence": 0.7})
        assert r1["active_tier"] == "tier1"

        # Tier2 arrives with higher confidence
        r2 = self.router.route(
            tier1={"gesture_id": 1, "confidence": 0.7},
            tier2={"gesture_id": 5, "confidence": 0.92},
        )
        assert r2["active_tier"] == "tier2"
        assert r2["gesture_id"] == 5

    def test_base_station_status_passthrough(self):
        """Verify base station status is available after parsing."""
        data = self._make_base_station_packet()
        parsed = self.parser.parse_base_station_packet(data)
        assert parsed["base_station"]["c6_connected"] is True
        assert parsed["base_station"]["active_tier"] == 2
