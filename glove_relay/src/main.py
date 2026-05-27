# -*- coding: utf-8 -*-
from __future__ import annotations

"""
glove_relay.src.main — FastAPI application entry point.

Starts the Relay server which:
  1. Listens on UDP :8888 for Protobuf sensor data from the ESP32 glove.
  2. Runs L1 (lightweight) and L2 (ST-GCN fallback) inference.
  3. Applies NLP grammar correction and optional TTS.
  4. Broadcasts JSON results to every connected WebSocket client on :8765.

Usage
-----
    uvicorn src.main:app --reload       # development
    uvicorn src.main:app --host 0.0.0.0 --port 8765  # production
    python -m src.main                    # equivalent (calls main())
"""


import asyncio
from contextlib import asynccontextmanager
from pathlib import Path
from typing import AsyncGenerator

from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from fastapi.middleware.cors import CORSMiddleware

from src.models.model_registry import ModelRegistry
from src.udp_server import UDPServer
from src.utils.config import get_config
from src.utils.logger import get_logger
from src.ws_server import ConnectionManager

logger = get_logger(__name__)

# ---------------------------------------------------------------------------
# Application-level singletons (initialised in lifespan)
# ---------------------------------------------------------------------------
ws_manager = ConnectionManager()
udp_server: UDPServer | None = None
model_registry: ModelRegistry | None = None


# ---------------------------------------------------------------------------
# Lifespan — startup / shutdown
# ---------------------------------------------------------------------------
@asynccontextmanager
async def lifespan(application: FastAPI) -> AsyncGenerator[None, None]:
    """Manage startup and shutdown lifecycle of the Relay server."""
    global udp_server, model_registry  # noqa: PLW0603

    config = get_config()
    logger.info("Relay server starting …")

    # --- Model registry --------------------------------------------------
    model_registry = ModelRegistry()
    model_registry.load_from_config()
    logger.info(
        "Models loaded — L1: %s | L2: %s",
        model_registry.active_l1_name,
        model_registry.active_l2_name,
    )

    # --- UDP server (background asyncio task) ----------------------------
    udp_server = UDPServer(
        host=config.udp.host,
        port=config.udp.port,
        buffer_size=config.udp.buffer_size,
        on_data_callback=ws_manager.broadcast,
    )
    udp_task = asyncio.create_task(udp_server.receive_loop(), name="udp-receive")
    logger.info("UDP server bound to %s:%d", config.udp.host, config.udp.port)

    yield  # application is now running

    # --- Shutdown --------------------------------------------------------
    logger.info("Relay server shutting down …")
    udp_server.running = False
    udp_task.cancel()
    try:
        await udp_task
    except asyncio.CancelledError:
        pass

    if model_registry is not None:
        model_registry.cleanup()

    await ws_manager.close_all()
    logger.info("Relay server stopped.")


# ---------------------------------------------------------------------------
# FastAPI application factory
# ---------------------------------------------------------------------------
app = FastAPI(
    title="Glove Relay V3",
    description="Bridges ESP32 protobuf UDP ↔ React WebSocket with L1/L2 inference, NLP, and TTS.",
    version="3.0.0",
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
    return {"status": "ok", "service": "glove-relay", "version": "3.0.0"}


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
