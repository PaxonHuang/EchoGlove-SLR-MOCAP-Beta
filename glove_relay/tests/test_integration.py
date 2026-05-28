# -*- coding: utf-8 -*-
"""Integration tests for the FastAPI relay application."""

from __future__ import annotations

import pytest
from unittest.mock import AsyncMock, MagicMock, patch

from httpx import ASGITransport, AsyncClient


# ---------------------------------------------------------------------------
# Fixtures
# ---------------------------------------------------------------------------
@pytest.fixture
def mock_config():
    """Provide a mock config that satisfies the lifespan startup."""
    cfg = MagicMock()
    cfg.udp.host = "127.0.0.1"
    cfg.udp.port = 18888
    cfg.udp.buffer_size = 4096
    cfg.websocket.host = "127.0.0.1"
    cfg.websocket.port = 18765
    cfg.cors.origins = ["*"]
    cfg.cors.allow_credentials = False
    cfg.cors.allow_methods = ["*"]
    cfg.cors.allow_headers = ["*"]
    cfg.inference.l1_confidence_threshold = 0.85
    cfg.inference.debounce_frames = 3
    cfg.inference.gesture_silence_ms = 100
    cfg.inference.l2_window_size = 5
    cfg.inference.l2_enabled = True
    cfg.nlp.enabled = False
    cfg.tts.enabled = False
    cfg.logging.level = "WARNING"
    cfg.models.config_path = "configs/model_config.yaml"
    return cfg


@pytest.fixture
def mock_model_registry():
    """Provide a mock ModelRegistry that doesn't load real models."""
    registry = MagicMock()
    registry.active_l1_name = "mock_l1"
    registry.active_l2_name = "mock_l2"
    registry.l1_model = None
    registry.l2_model = None
    registry.list_models.return_value = {
        "l1": {"name": "mock_l1", "type": "cnn_attention"},
        "l2": {"name": "mock_l2", "type": "stgcn"},
    }
    registry.switch.return_value = True
    registry.cleanup.return_value = None
    return registry


@pytest.fixture
async def app_client(mock_config, mock_model_registry):
    """Create an AsyncClient bound to the FastAPI app with mocked dependencies."""
    # Import the module so we can patch its globals directly
    import src.main as main_mod

    with (
        patch("src.main.get_config", return_value=mock_config),
        patch("src.main.UDPServer") as MockUDP,
    ):
        mock_udp_instance = MagicMock()
        mock_udp_instance.receive_loop = AsyncMock()
        mock_udp_instance.running = True
        MockUDP.return_value = mock_udp_instance

        # Directly set the module-level singletons that lifespan would create
        main_mod.model_registry = mock_model_registry
        main_mod.grammar_corrector = None
        main_mod.tts_engine = None

        from src.main import app

        transport = ASGITransport(app=app)
        async with AsyncClient(transport=transport, base_url="http://test") as client:
            yield client


# ---------------------------------------------------------------------------
# Health endpoint
# ---------------------------------------------------------------------------
class TestHealthEndpoint:
    @pytest.mark.asyncio
    async def test_health_returns_200(self, app_client: AsyncClient):
        resp = await app_client.get("/health")
        assert resp.status_code == 200

    @pytest.mark.asyncio
    async def test_health_body(self, app_client: AsyncClient):
        data = (await app_client.get("/health")).json()
        assert data["status"] == "ok"
        assert data["service"] == "glove-relay"
        assert "version" in data


# ---------------------------------------------------------------------------
# Status endpoint
# ---------------------------------------------------------------------------
class TestStatusEndpoint:
    @pytest.mark.asyncio
    async def test_status_returns_200(self, app_client: AsyncClient):
        resp = await app_client.get("/api/status")
        assert resp.status_code == 200

    @pytest.mark.asyncio
    async def test_status_has_expected_keys(self, app_client: AsyncClient):
        data = (await app_client.get("/api/status")).json()
        for key in ("uptime_s", "udp", "websocket", "ws_clients", "models", "nlp", "tts"):
            assert key in data, f"missing key: {key}"

    @pytest.mark.asyncio
    async def test_status_models(self, app_client: AsyncClient):
        data = (await app_client.get("/api/status")).json()
        assert data["models"]["l1"] == "mock_l1"
        assert data["models"]["l2"] == "mock_l2"

    @pytest.mark.asyncio
    async def test_status_ports(self, app_client: AsyncClient):
        data = (await app_client.get("/api/status")).json()
        assert data["udp"]["port"] == 18888
        assert data["websocket"]["port"] == 18765


# ---------------------------------------------------------------------------
# Models API
# ---------------------------------------------------------------------------
class TestModelsAPI:
    @pytest.mark.asyncio
    async def test_list_models(self, app_client: AsyncClient):
        resp = await app_client.get("/api/models")
        assert resp.status_code == 200
        data = resp.json()
        assert "l1" in data
        assert "l2" in data

    @pytest.mark.asyncio
    async def test_switch_model(self, app_client: AsyncClient):
        resp = await app_client.post("/api/models/switch/l1?model_name=new_model")
        assert resp.status_code == 200
        data = resp.json()
        assert data["status"] == "switched"


# ---------------------------------------------------------------------------
# TTS audio endpoint
# ---------------------------------------------------------------------------
class TestTTSAudioEndpoint:
    @pytest.mark.asyncio
    async def test_tts_returns_503_when_disabled(self, app_client: AsyncClient):
        """TTS engine is not loaded (config.tts.enabled=False) -> 503."""
        resp = await app_client.get("/api/tts/audio?text=hello")
        assert resp.status_code == 503


# ---------------------------------------------------------------------------
# OpenAPI / docs
# ---------------------------------------------------------------------------
class TestOpenAPI:
    @pytest.mark.asyncio
    async def test_openapi_json(self, app_client: AsyncClient):
        resp = await app_client.get("/openapi.json")
        assert resp.status_code == 200
        schema = resp.json()
        assert "paths" in schema
        assert "/health" in schema["paths"]
        assert "/api/status" in schema["paths"]
        assert "/api/tts/audio" in schema["paths"]
