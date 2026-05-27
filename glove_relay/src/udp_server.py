# -*- coding: utf-8 -*-
from __future__ import annotations

"""
glove_relay.src.udp_server — Asyncio UDP server for receiving ESP32 data.

Protocol flow
-------------
1. ESP32 sends *GloveData* protobuf datagrams to ``<host>:8888``.
2. ``UDPServer.receive_loop()`` parses each datagram via
   :func:`src.protobuf_parser.parse_glove_data`.
3. L1 inference is invoked immediately; if confidence ≤ threshold,
   frames are buffered and L2 inference is triggered.
4. The resulting JSON dict is pushed to all WebSocket clients via the
   ``on_data_callback``.
"""


import asyncio
import time
from collections import deque
from typing import Any, Callable, Deque

import numpy as np

from src.protobuf_parser import parse_glove_data
from src.utils.config import get_config
from src.utils.logger import get_logger

logger = get_logger(__name__)

# Type alias for the broadcast callback provided by the WS manager.
BroadcastFn = Callable[[dict[str, Any]], Any]


class UDPServer:
    """Asynchronous UDP receiver that drives the inference pipeline."""

    def __init__(
        self,
        host: str,
        port: int,
        buffer_size: int = 4096,
        on_data_callback: BroadcastFn | None = None,
    ) -> None:
        """
        Parameters
        ----------
        host:
            Bind address (typically ``"0.0.0.0"``).
        port:
            UDP port to listen on.
        buffer_size:
            Maximum datagram payload size.
        on_data_callback:
            Function called with a JSON-serialisable ``dict`` whenever a
            fully-processed inference result is available.
        """
        self.host = host
        self.port = port
        self.buffer_size = buffer_size
        self.on_data_callback: BroadcastFn | None = on_data_callback

        # asyncio transport / protocol objects
        self._transport: asyncio.DatagramTransport | None = None
        self._protocol: _UDPProtocol | None = None

        # Sliding window for L2 inference
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
            parsed = parse_glove_data(data)
        except Exception:
            logger.warning("Failed to parse datagram from %s:%d (%d bytes)", addr[0], addr[1], len(data))
            return

        # --- L1 inference (synchronous, lightweight) -------------------
        l1_result = self._run_l1(parsed)

        result: dict[str, Any] = {
            "timestamp": parsed.get("timestamp", time.time()),
            "hall": parsed.get("hall", []),
            "imu": parsed.get("imu", []),
            "l1_gesture_id": l1_result[0],
            "l1_confidence": l1_result[1],
            "l2_gesture_id": -1,
            "l2_confidence": 0.0,
            "nlp_text": "",
            "status": "l1_ok",
        }

        # --- Decide whether to trigger L2 --------------------------------
        now = time.time() * 1000.0  # ms
        if (
            self._config.inference.l2_enabled
            and l1_result[1] <= self._l2_threshold
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
    def _run_l1(self, parsed: dict[str, Any]) -> tuple[int, float]:
        """Run L1 model on a single frame. Returns ``(gesture_id, confidence)``."""
        # Lazy import to avoid circular dependency at module level
        from src.models.model_registry import ModelRegistry

        registry = ModelRegistry.instance()
        if registry is not None and registry.l1_model is not None:
            features = np.array(parsed.get("hall", []) + parsed.get("imu", []), dtype=np.float32)
            return registry.l1_model.predict(features.reshape(1, -1))
        # Fallback when no model is loaded
        logger.debug("No L1 model — returning placeholder")
        return (-1, 0.0)

    def _run_l2(self) -> tuple[int, float]:
        """Run L2 (ST-GCN) model on the buffered window. Returns ``(gesture_id, confidence)``."""
        from src.models.model_registry import ModelRegistry

        registry = ModelRegistry.instance()
        if registry is not None and registry.l2_model is not None:
            window = np.stack(
                [
                    np.array(f.get("hall", []) + f.get("imu", []), dtype=np.float32)
                    for f in self._frame_buffer
                ],
                axis=0,
            )  # (T, 21)
            return registry.l2_model.predict(window.unsqueeze(0) if hasattr(window, "unsqueeze") else window.reshape(1, *window.shape))
        logger.debug("No L2 model — returning placeholder")
        return (-1, 0.0)
