# -*- coding: utf-8 -*-
from __future__ import annotations

"""
glove_relay.src.utils.config — YAML configuration loader with typed access.

Usage
-----
    from src.utils.config import get_config
    cfg = get_config()
    print(cfg.udp.port)          # 8888
    print(cfg.inference.l1_confidence_threshold)  # 0.85
"""


import yaml
from dataclasses import dataclass, field
from pathlib import Path
from typing import List

from src.utils.logger import get_logger

logger = get_logger(__name__)

# Default paths relative to the project root
_PROJECT_ROOT = Path(__file__).resolve().parents[2]  # glove_relay/
_DEFAULT_RELAY_CFG = _PROJECT_ROOT / "configs" / "relay_config.yaml"
_DEFAULT_MODEL_CFG = _PROJECT_ROOT / "configs" / "model_config.yaml"


# ---------------------------------------------------------------------------
# Typed configuration sections
# ---------------------------------------------------------------------------
@dataclass
class _UDPConfig:
    host: str = "0.0.0.0"
    port: int = 8888
    buffer_size: int = 4096
    recv_timeout: float = 5.0


@dataclass
class _WebSocketConfig:
    host: str = "0.0.0.0"
    port: int = 8765
    max_connections: int = 10
    ping_interval: int = 30
    ping_timeout: int = 10


@dataclass
class _CORSConfig:
    origins: List[str] = field(default_factory=lambda: ["http://localhost:5173"])
    allow_credentials: bool = True
    allow_methods: List[str] = field(default_factory=lambda: ["*"])
    allow_headers: List[str] = field(default_factory=lambda: ["*"])


@dataclass
class _InferenceConfig:
    l1_confidence_threshold: float = 0.85
    l2_enabled: bool = True
    debounce_frames: int = 3
    gesture_silence_ms: int = 800
    l2_window_size: int = 30
    device: str = "auto"


@dataclass
class _LoggingConfig:
    level: str = "INFO"
    format: str = "%(asctime)s | %(levelname)-8s | %(name)s | %(message)s"
    file: str | None = None


@dataclass
class _NLPConfig:
    enabled: bool = True
    correction_rules: str = "configs/grammar_rules.yaml"


@dataclass
class _TTSConfig:
    enabled: bool = True
    voice: str = "zh-CN-XiaoxiaoNeural"
    rate: str = "+0%"
    volume: str = "+0%"
    cache_dir: str = "data/tts_cache"


@dataclass
class RelayConfig:
    """Top-level configuration container with typed sub-sections."""

    udp: _UDPConfig = field(default_factory=_UDPConfig)
    websocket: _WebSocketConfig = field(default_factory=_WebSocketConfig)
    cors: _CORSConfig = field(default_factory=_CORSConfig)
    inference: _InferenceConfig = field(default_factory=_InferenceConfig)
    logging: _LoggingConfig = field(default_factory=_LoggingConfig)
    nlp: _NLPConfig = field(default_factory=_NLPConfig)
    tts: _TTSConfig = field(default_factory=_TTSConfig)

    # Raw model config (loaded separately)
    model_config: dict | None = None


# ---------------------------------------------------------------------------
# Singleton accessor
# ---------------------------------------------------------------------------
_config_instance: RelayConfig | None = None


def _load_yaml(path: Path) -> dict:
    """Load a YAML file and return its contents as a dict."""
    if not path.exists():
        logger.warning("Config file not found: %s — using defaults", path)
        return {}
    with open(path, "r", encoding="utf-8") as fh:
        return yaml.safe_load(fh) or {}


def _build_config(relay_path: Path = _DEFAULT_RELAY_CFG) -> RelayConfig:
    """Build a ``RelayConfig`` from the YAML file."""
    raw = _load_yaml(relay_path)

    cfg = RelayConfig()

    # UDP
    if "udp" in raw:
        for k, v in raw["udp"].items():
            if hasattr(cfg.udp, k):
                setattr(cfg.udp, k, v)

    # WebSocket
    if "websocket" in raw:
        for k, v in raw["websocket"].items():
            if hasattr(cfg.websocket, k):
                setattr(cfg.websocket, k, v)

    # CORS
    if "cors" in raw:
        for k, v in raw["cors"].items():
            if hasattr(cfg.cors, k):
                setattr(cfg.cors, k, v)

    # Inference
    if "inference" in raw:
        for k, v in raw["inference"].items():
            if hasattr(cfg.inference, k):
                setattr(cfg.inference, k, v)

    # Logging
    if "logging" in raw:
        for k, v in raw["logging"].items():
            if hasattr(cfg.logging, k):
                setattr(cfg.logging, k, v)

    # NLP
    if "nlp" in raw:
        for k, v in raw["nlp"].items():
            if hasattr(cfg.nlp, k):
                setattr(cfg.nlp, k, v)

    # TTS
    if "tts" in raw:
        for k, v in raw["tts"].items():
            if hasattr(cfg.tts, k):
                setattr(cfg.tts, k, v)

    return cfg


def get_config() -> RelayConfig:
    """Return the singleton :class:`RelayConfig` instance."""
    global _config_instance  # noqa: PLW0603
    if _config_instance is None:
        _config_instance = _build_config()
    return _config_instance


def reload_config(relay_path: Path = _DEFAULT_RELAY_CFG) -> RelayConfig:
    """Force-reload the configuration from disk and return it."""
    global _config_instance  # noqa: PLW0603
    _config_instance = _build_config(relay_path)
    return _config_instance


def load_model_config(path: Path = _DEFAULT_MODEL_CFG) -> dict:
    """Load and return the raw model configuration dict."""
    return _load_yaml(path)
