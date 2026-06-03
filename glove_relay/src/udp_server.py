# -*- coding: utf-8 -*-
from __future__ import annotations

"""
glove_relay.src.udp_server — Asyncio UDP server for receiving V5 receiver data.

Protocol flow (V5)
------------------
1. Receiver ESP32 sends *ReceiverPacket* protobuf datagrams to ``<host>:8888``.
2. ``UDPServer.receive_loop()`` parses each datagram via
   :class:`src.protobuf_parser.ProtobufParser`.
3. Tier1/Tier2 inference is invoked; results are routed via ConfidenceRouter.
4. The resulting JSON dict is pushed to all WebSocket clients via the
   ``on_data_callback``.
"""

import asyncio
import time
from collections import deque
from typing import Any, Callable, Deque

import numpy as np

from src.confidence_router import ConfidenceRouter
from src.protobuf_parser import ProtobufParser
from src.utils.config import get_config
from src.utils.logger import get_logger

logger = get_logger(__name__)

# Type alias for the broadcast callback provided by the WS manager.
BroadcastFn = Callable[[dict[str, Any]], Any]


class UDPServer:
    """Asynchronous UDP receiver that drives the V5 inference pipeline."""

    def __init__(
        self,
        host: str,
        port: int,
        buffer_size: int = 4096,
        on_data_callback: BroadcastFn | None = None,
        router: ConfidenceRouter | None = None,
    ) -> None:
        self.host = host
        self.port = port
        self.buffer_size = buffer_size
        self.on_data_callback: BroadcastFn | None = on_data_callback
        self._router: ConfidenceRouter | None = router
        self._parser = ProtobufParser()

        # asyncio transport / protocol objects
        self._transport: asyncio.DatagramTransport | None = None
        self._protocol: _UDPProtocol | None = None

        # Sliding window for Tier2 inference
        self._config = get_config()
        self._window_size: int = self._config.inference.l2_window_size  # 30
        self._frame_buffer: Deque[dict[str, Any]] = deque(maxlen=self._window_size)

        # Debounce / silence tracking
        self._debounce_counter: int = 0
        self._last_gesture_time: float = 0.0
        self._debounce_frames: int = self._config.inference.debounce_frames
        self._silence_ms: int = self._config.inference.gesture_silence_ms
        self._l2_threshold: float = self._config.inference.l1_confidence_threshold

        # Public flag — set to ``False`` to stop the receive loop.
        self.running: bool = True

    # ------------------------------------------------------------------
    # Public async API
    # ------------------------------------------------------------------
    async def receive_loop(self) -> None:
        """Create the UDP endpoint and enter the receive loop."""
        loop = asyncio.get_running_loop()

        class _Proto(asyncio.DatagramProtocol):
            """Trivial protocol that forwards datagrams to the server."""

            def __init__(self, parent: UDPServer) -> None:
                self.parent = parent

            def datagram_received(self, data: bytes, addr: tuple[str, int]) -> None:
                """Handle an incoming datagram."""
                self.parent._handle_datagram(data, addr)  # noqa: SLF001

            def connection_made(self, transport: asyncio.BaseTransport) -> None:
                self.parent._transport = transport  # noqa: SLF001

            def error_received(self, exc: Exception) -> None:
                logger.error("UDP error: %s", exc)

        self._protocol = _Proto(self)
        await loop.create_datagram_endpoint(
            lambda: self._protocol,
            local_addr=(self.host, self.port),
        )
        logger.info("UDP receive_loop running on %s:%d", self.host, self.port)

        # The loop is driven by the protocol callbacks; just sleep.
        try:
            while self.running:
                await asyncio.sleep(0.1)
        except asyncio.CancelledError:
            logger.info("UDP receive_loop cancelled")
        finally:
            if self._transport is not None:
                self._transport.close()

    # ------------------------------------------------------------------
    # Internal processing
    # ------------------------------------------------------------------
    def _handle_datagram(self, data: bytes, addr: tuple[str, int]) -> None:
        """Parse one datagram, run inference, and broadcast results."""
        try:
            parsed = self._parser.parse_receiver_packet(data)
            if parsed is None:
                logger.warning("Invalid V5 packet from %s:%d (%d bytes)", addr[0], addr[1], len(data))
                return
        except Exception:
            logger.warning("Failed to parse datagram from %s:%d (%d bytes)", addr[0], addr[1], len(data))
            return

        # --- Build tier results from parsed V5 data ---
        left = parsed.get("left", {})
        right = parsed.get("right", {})

        tier1_result = {
            "gesture_id": left.get("l1_gesture_id", -1),
            "confidence": left.get("l1_confidence", 0.0),
        }

        # --- Use ConfidenceRouter if available ---
        if self._router is not None:
            route = self._router.route(tier1=tier1_result)
            result: dict[str, Any] = {
                "timestamp": time.time(),
                "flex": left.get("flex", []),
                "imu": left.get("imu", []),
                "l1_gesture_id": tier1_result["gesture_id"],
                "l1_confidence": tier1_result["confidence"],
                "l2_gesture_id": route.get("gesture_id", -1) if route.get("active_tier") != "tier1" else -1,
                "l2_confidence": route.get("confidence", 0.0) if route.get("active_tier") != "tier1" else 0.0,
                "nlp_text": "",
                "status": f"v5_{route.get('active_tier', 'tier1')}",
            }
        else:
            # Legacy path (no router)
            result = {
                "timestamp": time.time(),
                "flex": left.get("flex", []),
                "imu": left.get("imu", []),
                "l1_gesture_id": tier1_result["gesture_id"],
                "l1_confidence": tier1_result["confidence"],
                "l2_gesture_id": -1,
                "l2_confidence": 0.0,
                "nlp_text": "",
                "status": "l1_ok",
            }

            now = time.time() * 1000.0
            if (
                self._config.inference.l2_enabled
                and tier1_result["confidence"] <= self._l2_threshold
                and (now - self._last_gesture_time) >= self._silence_ms
                and self._debounce_counter >= self._debounce_frames
            ):
                self._frame_buffer.append(parsed)
                self._debounce_counter = 0

                if len(self._frame_buffer) >= self._window_size:
                    l2_result = self._run_l2()
                    result["l2_gesture_id"] = l2_result[0]
                    result["l2_confidence"] = l2_result[1]
                    result["status"] = "l2_ok"
                    self._frame_buffer.clear()
                    self._last_gesture_time = now
            else:
                self._debounce_counter += 1

        # --- Broadcast to WebSocket clients -----------------------------
        if self.on_data_callback is not None:
            try:
                self.on_data_callback(result)
            except Exception:
                logger.exception("Broadcast callback failed")

    # ------------------------------------------------------------------
    # Inference stubs — replaced with real model calls in production
    # ------------------------------------------------------------------
    def _run_l1(self, features: np.ndarray) -> tuple[int, float]:
        """Run L1 model on a single frame. Returns ``(gesture_id, confidence)``."""
        from src.models.model_registry import ModelRegistry

        registry = ModelRegistry.instance()
        if registry is not None and registry.l1_model is not None:
            return registry.l1_model.predict(features.reshape(1, -1))
        logger.debug("No L1 model — returning placeholder")
        return (-1, 0.0)

    def _run_l2(self) -> tuple[int, float]:
        """Run L2 (ST-GCN) model on the buffered window. Returns ``(gesture_id, confidence)``."""
        from src.models.model_registry import ModelRegistry

        registry = ModelRegistry.instance()
        if registry is not None and registry.l2_model is not None:
            window = np.stack(
                [
                    np.array(f.get("left", {}).get("flex", []) + f.get("left", {}).get("imu", []), dtype=np.float32)
                    for f in self._frame_buffer
                ],
                axis=0,
            )  # (T, 11)
            return registry.l2_model.predict(window.reshape(1, *window.shape))
        logger.debug("No L2 model — returning placeholder")
        return (-1, 0.0)
