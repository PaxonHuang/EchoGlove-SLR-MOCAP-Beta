"""Tests for USB CDC serial input from P4 base station."""
from __future__ import annotations
import pytest
from unittest.mock import MagicMock, patch
from src.protobuf_parser import ProtobufParser


class TestBaseStationPacketParsing:
    def setup_method(self):
        self.parser = ProtobufParser()

    def test_parse_base_station_packet(self):
        from proto.glove_data_pb2 import BaseStationPacket
        bsp = BaseStationPacket()
        bsp.version = 5
        bsp.tick_id = 100
        bsp.left.flex.extend([0.1, 0.2, 0.3, 0.4, 0.5])
        bsp.left.imu.extend([1.0, 2.0, 3.0, 0.1, 0.2, 0.3])
        bsp.right.flex.extend([0.6, 0.7, 0.8, 0.9, 1.0])
        bsp.right.imu.extend([4.0, 5.0, 6.0, 0.4, 0.5, 0.6])
        bsp.relative_features.extend([0.1, 0.2, 0.3, 0.4, 0.5, 0.6])
        bsp.tier2_gesture_id = 5
        bsp.tier2_confidence = 0.92
        bsp.base_station.c6_connected = True
        bsp.base_station.p4_ready = True
        bsp.base_station.active_tier = 2

        data = bsp.SerializeToString()
        parsed = self.parser.parse_base_station_packet(data)
        assert parsed is not None
        assert parsed["version"] == 5
        assert parsed["tick_id"] == 100
        assert len(parsed["left"]["flex"]) == 5
        assert parsed["tier2_gesture_id"] == 5
        assert parsed["tier2_confidence"] == pytest.approx(0.92, abs=0.01)
        assert parsed["base_station"]["c6_connected"] is True

    def test_parse_base_station_packet_28dim(self):
        from proto.glove_data_pb2 import BaseStationPacket
        bsp = BaseStationPacket()
        bsp.version = 5
        bsp.left.flex.extend([0.1] * 5)
        bsp.left.imu.extend([1.0] * 6)
        bsp.right.flex.extend([0.5] * 5)
        bsp.right.imu.extend([2.0] * 6)
        bsp.relative_features.extend([0.01] * 6)

        data = bsp.SerializeToString()
        parsed = self.parser.parse_base_station_packet(data)
        features = self.parser.assemble_28dim_from_bsp(parsed)
        assert len(features) == 28
        assert features[0] == pytest.approx(0.1)
        assert features[11] == pytest.approx(0.5)

    def test_parse_base_station_packet_bad_version(self):
        from proto.glove_data_pb2 import BaseStationPacket
        bsp = BaseStationPacket()
        bsp.version = 99
        data = bsp.SerializeToString()
        parsed = self.parser.parse_base_station_packet(data)
        assert parsed is None

    def test_parse_base_station_packet_empty_bytes(self):
        parsed = self.parser.parse_base_station_packet(b"")
        assert parsed is None


class TestUSBCDCServer:
    def test_usb_cdc_server_instantiation(self):
        from src.usb_cdc_server import USBCDCServer
        server = USBCDCServer(port="/dev/null", baud=2000000)
        assert server is not None
