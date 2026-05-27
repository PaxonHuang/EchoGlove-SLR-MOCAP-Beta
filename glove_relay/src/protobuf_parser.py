# -*- coding: utf-8 -*-
from __future__ import annotations

"""
glove_relay.src.protobuf_parser — Parse GloveData Protobuf messages.

The ESP32 firmware serialises sensor readings using ``glove_data.proto``
(package ``data_glove``).  This module converts raw bytes into a plain
``dict`` suitable for JSON serialisation over WebSocket.

.. note::
    The compiled ``_pb2`` module lives at ``proto/glove_data_pb2.py``.
    Regenerate with::

        python -m grpc_tools.protoc -Iproto --python_out=proto proto/glove_data.proto
"""


from typing import Any

from src.utils.logger import get_logger

logger = get_logger(__name__)

# ---------------------------------------------------------------------------
# Attempt to import the generated protobuf module.
# ---------------------------------------------------------------------------
try:
    from proto import glove_data_pb2  # type: ignore[import-untyped]

    _HAS_PROTOBUF = True
except ImportError:
    _HAS_PROTOBUF = False
    logger.warning(
        "proto/glove_data_pb2.py not found — using fallback parser.  "
        "Regenerate with: python -m grpc_tools.protoc -Iproto --python_out=proto proto/glove_data.proto"
    )


def parse_glove_data(data: bytes) -> dict[str, Any]:
    """
    Parse a single GloveData protobuf message into a dictionary.

    Parameters
    ----------
    data:
        Raw protobuf-encoded bytes received via UDP.

    Returns
    -------
    dict
        Keys:

        * ``timestamp`` (*int*) — microsecond timestamp from ESP32.
        * ``hall`` (*list[float]*) — 15 hall sensor values (5 × 3 axes).
        * ``imu`` (*list[float]*) — 6 IMU values (3 euler + 3 gyro).
        * ``flex`` (*list[float]*) — 5 flex sensor values (placeholder).
        * ``l1_gesture_id`` (*int*) — on-device gesture prediction.
        * ``l1_confidence`` (*float*) — prediction confidence [0, 1].
        * ``l2_requested`` (*bool*) — whether L2 inference is needed.
        * ``status`` (*str*) — system status string.

    Raises
    ------
    ValueError
        If *data* cannot be decoded as a valid ``GloveData`` message.
    """
    if _HAS_PROTOBUF:
        return _parse_with_pb2(data)
    return _parse_fallback(data)


# ---------------------------------------------------------------------------
# Full protobuf decoder
# ---------------------------------------------------------------------------
def _parse_with_pb2(data: bytes) -> dict[str, Any]:
    """Decode using the compiled ``glove_data_pb2`` module."""
    msg = glove_data_pb2.GloveData()
    try:
        msg.ParseFromString(data)
    except Exception as exc:
        raise ValueError(f"Protobuf decode error: {exc}") from exc

    return {
        "timestamp": msg.timestamp,
        "hall": list(msg.hall_features),
        "imu": list(msg.imu_features),
        "flex": list(msg.flex_features),
        "l1_gesture_id": msg.l1_gesture_id,
        "l1_confidence": msg.l1_confidence,
        "l2_requested": msg.l2_requested,
        "status": msg.status,
    }


# ---------------------------------------------------------------------------
# Minimal fallback parser (binary struct layout matching the protobuf wire)
# ---------------------------------------------------------------------------
def _parse_fallback(data: bytes) -> dict[str, Any]:
    """
    Best-effort binary parser when protobuf is unavailable.

    Expected layout (V3 firmware):
        [timestamp:     4B uint32 LE]
        [hall_features: 15 × 4B float32 LE]
        [imu_features:   6 × 4B float32 LE]
        [flex_features:  5 × 4B float32 LE]
        [l1_gesture_id: 4B uint32 LE]
        [l1_confidence: 4B float32 LE]
        [l2_requested:  1B bool]
        [status_len:    4B uint32 LE]
        [status:        N bytes UTF-8]

    This is a safety net only — prefer protobuf decoding.
    """
    import struct
    import time

    min_size = 4 + 15 * 4 + 6 * 4 + 5 * 4 + 4 + 4 + 1 + 4
    if len(data) < min_size:
        raise ValueError(f"Fallback parser: expected ≥{min_size} bytes, got {len(data)}")

    offset = 0

    timestamp = struct.unpack_from("<I", data, offset)[0]
    offset += 4

    hall = list(struct.unpack_from("<15f", data, offset))
    offset += 15 * 4

    imu = list(struct.unpack_from("<6f", data, offset))
    offset += 6 * 4

    flex = list(struct.unpack_from("<5f", data, offset))
    offset += 5 * 4

    l1_gesture_id = struct.unpack_from("<I", data, offset)[0]
    offset += 4

    l1_confidence = struct.unpack_from("<f", data, offset)[0]
    offset += 4

    l2_requested = bool(data[offset])
    offset += 1

    status_len = struct.unpack_from("<I", data, offset)[0]
    offset += 4
    status = data[offset : offset + status_len].decode("utf-8", errors="replace")

    if timestamp == 0:
        timestamp = int(time.time() * 1_000_000)

    return {
        "timestamp": timestamp,
        "hall": hall,
        "imu": imu,
        "flex": flex,
        "l1_gesture_id": l1_gesture_id,
        "l1_confidence": l1_confidence,
        "l2_requested": l2_requested,
        "status": status,
    }


def build_glove_data_dict(
    timestamp: int = 0,
    hall: list[float] | None = None,
    imu: list[float] | None = None,
    flex: list[float] | None = None,
    l1_gesture_id: int = 0,
    l1_confidence: float = 0.0,
    l2_requested: bool = False,
    status: str = "IDLE",
) -> dict[str, Any]:
    """Build a canonical GloveData dict (for testing / simulation)."""
    return {
        "timestamp": timestamp,
        "hall": hall if hall is not None else [0.0] * 15,
        "imu": imu if imu is not None else [0.0] * 6,
        "flex": flex if flex is not None else [0.0] * 5,
        "l1_gesture_id": l1_gesture_id,
        "l1_confidence": l1_confidence,
        "l2_requested": l2_requested,
        "status": status,
    }
