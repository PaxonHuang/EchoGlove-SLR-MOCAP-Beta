"""V5 Protobuf parser for EchoGlove dual-hand data."""
from __future__ import annotations
from typing import Optional
from proto.glove_data_pb2 import GloveData, ReceiverPacket, BaseStationPacket


class ProtobufParser:
    def __init__(self, expected_version: int = 5):
        self._expected_version = expected_version

    def parse_receiver_packet(self, data: bytes) -> Optional[dict]:
        try:
            rp = ReceiverPacket()
            rp.ParseFromString(data)
            if rp.version != self._expected_version:
                return None
            return {
                "version": rp.version,
                "tick_id": rp.tick_id,
                "left": self._parse_glove_data(rp.left),
                "right": self._parse_glove_data(rp.right),
                "relative_features": list(rp.relative_features),
            }
        except Exception:
            return None

    def _parse_glove_data(self, gd: GloveData) -> dict:
        return {
            "version": gd.version,
            "hand_id": gd.hand_id,
            "flex": list(gd.flex),
            "imu": list(gd.imu),
            "l1_gesture_id": gd.l1_gesture_id,
            "l1_confidence": gd.l1_confidence,
            "relative": list(gd.relative),
            "tier2_gesture_id": gd.tier2_gesture_id,
            "tier2_confidence": gd.tier2_confidence,
            "status": gd.status,
        }

    def assemble_28dim(self, parsed: dict) -> list[float]:
        """Assemble 28-dim feature vector: left[11] + right[11] + relative[6]"""
        left = parsed["left"]
        right = parsed["right"]
        features = []
        features.extend(left["flex"])
        features.extend(left["imu"][:6])
        features.extend(right["flex"])
        features.extend(right["imu"][:6])
        features.extend(parsed["relative_features"])
        return features

    def parse_base_station_packet(self, data: bytes) -> Optional[dict]:
        """Parse a BaseStationPacket protobuf from the P4 base station."""
        try:
            bsp = BaseStationPacket()
            bsp.ParseFromString(data)
            if bsp.version != self._expected_version:
                return None
            bs_status = {
                "c6_connected": bsp.base_station.c6_connected,
                "p4_ready": bsp.base_station.p4_ready,
                "cpu_usage": bsp.base_station.cpu_usage,
                "mem_usage": bsp.base_station.mem_usage,
                "uptime_s": bsp.base_station.uptime_s,
                "active_tier": bsp.base_station.active_tier,
            }
            return {
                "version": bsp.version,
                "tick_id": bsp.tick_id,
                "left": self._parse_glove_data(bsp.left),
                "right": self._parse_glove_data(bsp.right),
                "relative_features": list(bsp.relative_features),
                "tier2_gesture_id": bsp.tier2_gesture_id,
                "tier2_confidence": bsp.tier2_confidence,
                "base_station": bs_status,
            }
        except Exception:
            return None

    def assemble_28dim_from_bsp(self, parsed: dict) -> list[float]:
        """Assemble 28-dim feature vector from a BaseStationPacket parse result.

        This delegates to ``assemble_28dim`` — the dictionary layout is identical.
        """
        return self.assemble_28dim(parsed)
