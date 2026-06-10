# -*- coding: utf-8 -*-
from __future__ import annotations

"""
glove_relay.src.main — FastAPI application entry point (V5).

Starts the Relay server which:
  1. Listens on UDP :8888 for V5 ReceiverPacket protobuf from the ESP32 receiver.
  2. Runs Tier1/Tier2/Tier3 inference with ConfidenceRouter.
  3. Applies NLP grammar correction and optional TTS.
  4. Broadcasts JSON results to every connected WebSocket client on :8765.

Usage
-----
    uvicorn src.main:app --reload       # development
    uvicorn src.main:app --host 0.0.0.0 --port 8765  # production
    python -m src.main                    # equivalent (calls main())
"""


import asyncio
import time
from contextlib import asynccontextmanager
from typing import AsyncGenerator

from fastapi import FastAPI, Query, WebSocket, WebSocketDisconnect
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import Response

from src.confidence_router import ConfidenceRouter
from src.models.model_registry import ModelRegistry
from src.nlp.grammar_corrector import GrammarCorrector
from src.protobuf_parser import ProtobufParser
from src.tts.tts_engine import TTSEngine
from src.udp_server import UDPServer
from src.usb_cdc_server import USBCDCServer
from src.utils.config import get_config
from src.utils.logger import get_logger
from src.ws_server import ConnectionManager

logger = get_logger(__name__)

# ---------------------------------------------------------------------------
# Application-level singletons (initialised in lifespan)
# ---------------------------------------------------------------------------
ws_manager = ConnectionManager()
udp_server: UDPServer | None = None
usb_cdc_server: USBCDCServer | None = None
model_registry: ModelRegistry | None = None
grammar_corrector: GrammarCorrector | None = None
tts_engine: TTSEngine | None = None
_start_time: float = 0.0


# ---------------------------------------------------------------------------
# USB CDC data handler
# ---------------------------------------------------------------------------
_bsp_parser = ProtobufParser()
_cdc_router: ConfidenceRouter | None = None


def _handle_usb_data(data: bytes) -> None:
    """Process raw bytes from USB CDC: parse BaseStationPacket, route, broadcast."""
    parsed = _bsp_parser.parse_base_station_packet(data)
    if parsed is None:
        logger.warning("USB CDC: failed to parse BaseStationPacket (%d bytes)", len(data))
        return

    left = parsed.get("left", {})
    right = parsed.get("right", {})

    tier1_result = {
        "gesture_id": left.get("l1_gesture_id", -1),
        "confidence": left.get("l1_confidence", 0.0),
    }
    tier2_result = {
        "gesture_id": parsed.get("tier2_gesture_id", -1),
        "confidence": parsed.get("tier2_confidence", 0.0),
    }

    if _cdc_router is not None:
        route = _cdc_router.route(tier1=tier1_result, tier2=tier2_result)
    else:
        route = {
            "gesture_id": tier2_result["gesture_id"] if tier2_result["confidence"] > 0 else tier1_result["gesture_id"],
            "confidence": max(tier1_result["confidence"], tier2_result["confidence"]),
            "active_tier": "tier2" if tier2_result["confidence"] > tier1_result["confidence"] else "tier1",
            "blend_alpha": 1.0,
        }

    result = {
        "timestamp": time.time(),
        "flex": left.get("flex", []),
        "imu": left.get("imu", []),
        "l1_gesture_id": tier1_result["gesture_id"],
        "l1_confidence": tier1_result["confidence"],
        "l2_gesture_id": tier2_result["gesture_id"],
        "l2_confidence": tier2_result["confidence"],
        "nlp_text": "",
        "status": f"bs_{route.get('active_tier', 'tier1')}",
        "base_station": parsed.get("base_station", {}),
    }

    try:
        ws_manager.broadcast(result)
    except Exception:
        logger.exception("USB CDC broadcast failed")


# ---------------------------------------------------------------------------
# Lifespan — startup / shutdown
# ---------------------------------------------------------------------------
@asynccontextmanager
async def lifespan(application: FastAPI) -> AsyncGenerator[None, None]:
    """Manage startup and shutdown lifecycle of the Relay server."""
    global udp_server, usb_cdc_server, model_registry, grammar_corrector, tts_engine, _start_time  # noqa: PLW0603
    global _cdc_router  # noqa: PLW0603

    config = get_config()
    _start_time = time.time()
    logger.info("Relay server starting …")

    # --- Model registry --------------------------------------------------
    model_registry = ModelRegistry()
    model_registry.load_from_config()
    logger.info(
        "Models loaded — L1: %s | L2: %s",
        model_registry.active_l1_name,
        model_registry.active_l2_name,
    )

    # --- NLP grammar corrector -------------------------------------------
    if config.nlp.enabled:
        grammar_corrector = GrammarCorrector()
        logger.info("NLP grammar corrector loaded")

    # --- TTS engine ------------------------------------------------------
    if config.tts.enabled:
        tts_engine = TTSEngine()
        logger.info("TTS engine loaded — voice=%s", tts_engine.voice)

    # --- Confidence router (V5) ------------------------------------------
    router = ConfidenceRouter(
        blend_frames=5,
        min_confidence=config.inference.l1_confidence_threshold,
    )

    # --- UDP server (background asyncio task) ----------------------------
    udp_server = UDPServer(
        host=config.udp.host,
        port=config.udp.port,
        buffer_size=config.udp.buffer_size,
        on_data_callback=ws_manager.broadcast,
        router=router,
    )
    udp_task = asyncio.create_task(udp_server.receive_loop(), name="udp-receive")
    logger.info("UDP server bound to %s:%d", config.udp.host, config.udp.port)

    # --- USB CDC server (P4 base station) ---------------------------------
    _cdc_router = router
    usb_port = getattr(config, "usb_cdc_port", "/dev/ttyACM0")
    usb_baud = getattr(config, "usb_cdc_baud", 2000000)
    usb_cdc_server = USBCDCServer(port=usb_port, baud=usb_baud)
    usb_cdc_server.set_callback(_handle_usb_data)
    usb_task = asyncio.create_task(usb_cdc_server.start(), name="usb-cdc")
    logger.info("USB CDC server started on %s @ %d", usb_port, usb_baud)

    yield  # application is now running

    # --- Shutdown --------------------------------------------------------
    logger.info("Relay server shutting down …")
    udp_server.running = False
    udp_task.cancel()
    try:
        await udp_task
    except asyncio.CancelledError:
        pass

    if usb_cdc_server is not None:
        usb_cdc_server.stop()
    usb_task.cancel()
    try:
        await usb_task
    except asyncio.CancelledError:
        pass

    if model_registry is not None:
        model_registry.cleanup()

    if tts_engine is not None:
        tts_engine.clear_cache()

    await ws_manager.close_all()
    logger.info("Relay server stopped.")


# ---------------------------------------------------------------------------
# FastAPI application factory
# ---------------------------------------------------------------------------
app = FastAPI(
    title="Glove Relay V5",
    description="Bridges ESP32-S3 receiver → PC relay with Tier1/Tier2/Tier3 inference, NLP, and TTS.",
    version="5.0.0",
    lifespan=lifespan,
)

# CORS
_config = get_config()
for _origin in _config.cors.origins:
    logger.debug("CORS origin allowed: %s", _origin)

app.add_middleware(
    CORSMiddleware,
    allow_origins=_config.cors.origins,
    allow_credentials=_config.cors.allow_credentials,
    allow_methods=_config.cors.allow_methods,
    allow_headers=_config.cors.allow_headers,
)


# ---------------------------------------------------------------------------
# Endpoints
# ---------------------------------------------------------------------------
@app.get("/health")
async def health_check() -> dict[str, str]:
    """Lightweight liveness probe."""
    return {"status": "ok", "service": "glove-relay", "version": "5.0.0"}


@app.get("/api/models")
async def list_models() -> dict:
    """Return currently loaded models and their metadata."""
    if model_registry is None:
        return {"error": "model registry not initialised"}
    return model_registry.list_models()


@app.post("/api/models/switch/{level}")
async def switch_model(level: str, model_name: str) -> dict:
    """Hot-switch the active model for *level* (``l1`` or ``l2``)."""
    if model_registry is None:
        return {"error": "model registry not initialised"}
    ok = model_registry.switch(level, model_name)
    if ok:
        return {"status": "switched", "level": level, "model": model_name}
    return {"error": f"switch failed for {level}/{model_name}"}


@app.websocket("/ws")
async def websocket_endpoint(websocket: WebSocket) -> None:
    """
    Accept a WebSocket connection and keep it alive.

    The client will receive JSON payloads broadcast by the UDP→inference
    pipeline whenever new sensor data arrives.
    """
    await ws_manager.connect(websocket)
    try:
        while True:
            # Keep the connection open; discard any client messages for now.
            _msg = await websocket.receive_text()
            logger.debug("WS recv (discarded): %s", _msg[:80])
    except WebSocketDisconnect:
        ws_manager.disconnect(websocket)
        logger.info("WebSocket client disconnected (%d remaining)", ws_manager.active_count)


# ---------------------------------------------------------------------------
# REST API — status & TTS audio
# ---------------------------------------------------------------------------
@app.get("/api/status")
async def api_status() -> dict:
    """Return system state: uptime, ports, models, NLP/TTS status."""
    config = get_config()
    uptime_s = time.time() - _start_time if _start_time else 0.0

    l1_name = model_registry.active_l1_name if model_registry else "none"
    l2_name = model_registry.active_l2_name if model_registry else "none"

    return {
        "uptime_s": round(uptime_s, 1),
        "udp": {"host": config.udp.host, "port": config.udp.port},
        "websocket": {"host": config.websocket.host, "port": config.websocket.port},
        "ws_clients": ws_manager.active_count,
        "models": {"l1": l1_name, "l2": l2_name},
        "nlp": {"enabled": config.nlp.enabled, "loaded": grammar_corrector is not None},
        "tts": {"enabled": config.tts.enabled, "loaded": tts_engine is not None},
    }


@app.get("/api/tts/audio")
async def api_tts_audio(text: str = Query(..., min_length=1, max_length=200)) -> Response:
    """Synthesize *text* to MP3 audio and return the bytes."""
    if tts_engine is None:
        return Response(content=b"", status_code=503, media_type="text/plain")
    audio = await tts_engine.synthesize(text)
    if not audio:
        return Response(content=b"", status_code=204, media_type="text/plain")
    return Response(content=audio, media_type="audio/mpeg")


# ---------------------------------------------------------------------------
# Uvicorn runner (invoked as `python -m src.main`)
# ---------------------------------------------------------------------------
def main() -> None:
    """Run the Relay server with uvicorn."""
    config = get_config()
    import uvicorn

    uvicorn.run(
        "src.main:app",
        host=config.websocket.host,
        port=config.websocket.port,
        reload=False,
        log_level=config.logging.level.lower(),
    )


if __name__ == "__main__":
    main()
