# -*- coding: utf-8 -*-
"""Tests for TTSEngine — edge-tts integration with caching."""

import sys
import pytest
from pathlib import Path
from types import ModuleType
from unittest.mock import AsyncMock, MagicMock, patch

from src.tts.tts_engine import TTSEngine


# ---------------------------------------------------------------------------
# Fixtures
# ---------------------------------------------------------------------------
@pytest.fixture
def engine(tmp_path: Path) -> TTSEngine:
    """Create a TTSEngine with a temporary cache directory."""
    with patch("src.tts.tts_engine.get_config") as mock_cfg:
        cfg = MagicMock()
        cfg.tts.voice = "zh-CN-XiaoxiaoNeural"
        cfg.tts.rate = "+0%"
        cfg.tts.volume = "+0%"
        cfg.tts.cache_dir = str(tmp_path / "tts_cache")
        mock_cfg.return_value = cfg
        return TTSEngine()


@pytest.fixture
def mock_edge_tts():
    """Mock the edge_tts module so tests don't hit the network."""
    mock_audio = b"\xff\xfb\x90\x00" + b"\x00" * 100  # fake MP3 header

    async def fake_stream():
        yield {"type": "audio", "data": mock_audio}

    mock_communicate = MagicMock()
    mock_communicate.return_value.stream = fake_stream

    # Create a fake module and inject into sys.modules
    fake_module = ModuleType("edge_tts")
    fake_module.Communicate = mock_communicate  # type: ignore[attr-defined]
    sys.modules["edge_tts"] = fake_module

    yield fake_module, mock_audio

    sys.modules.pop("edge_tts", None)


# ---------------------------------------------------------------------------
# Empty / whitespace input
# ---------------------------------------------------------------------------
class TestEmptyInput:
    @pytest.mark.asyncio
    async def test_empty_string_returns_empty(self, engine: TTSEngine):
        result = await engine.synthesize("")
        assert result == b""

    @pytest.mark.asyncio
    async def test_whitespace_returns_empty(self, engine: TTSEngine):
        result = await engine.synthesize("   ")
        assert result == b""


# ---------------------------------------------------------------------------
# Cache key determinism
# ---------------------------------------------------------------------------
class TestCacheKey:
    def test_same_text_same_key(self):
        k1 = TTSEngine._cache_key("你好", "zh-CN-XiaoxiaoNeural")
        k2 = TTSEngine._cache_key("你好", "zh-CN-XiaoxiaoNeural")
        assert k1 == k2

    def test_different_text_different_key(self):
        k1 = TTSEngine._cache_key("你好", "zh-CN-XiaoxiaoNeural")
        k2 = TTSEngine._cache_key("再见", "zh-CN-XiaoxiaoNeural")
        assert k1 != k2

    def test_different_voice_different_key(self):
        k1 = TTSEngine._cache_key("你好", "zh-CN-XiaoxiaoNeural")
        k2 = TTSEngine._cache_key("你好", "zh-CN-YunxiNeural")
        assert k1 != k2

    def test_key_is_hex_string(self):
        key = TTSEngine._cache_key("test", "voice")
        assert all(c in "0123456789abcdef" for c in key)
        assert len(key) == 16


# ---------------------------------------------------------------------------
# Synthesis with mocked edge_tts
# ---------------------------------------------------------------------------
class TestSynthesis:
    @pytest.mark.asyncio
    async def test_synthesize_returns_bytes(self, engine: TTSEngine, mock_edge_tts):
        _, expected_audio = mock_edge_tts
        result = await engine.synthesize("你好")
        assert isinstance(result, bytes)
        assert len(result) > 0

    @pytest.mark.asyncio
    async def test_synthesize_writes_cache(self, engine: TTSEngine, mock_edge_tts):
        await engine.synthesize("你好世界")
        cache_files = list(engine.cache_dir.glob("*.mp3"))
        assert len(cache_files) == 1

    @pytest.mark.asyncio
    async def test_cache_hit_skips_synthesis(self, engine: TTSEngine, mock_edge_tts):
        mock_module, _ = mock_edge_tts
        # First call — should invoke edge_tts
        await engine.synthesize("缓存测试")
        # Second call — should hit cache
        result = await engine.synthesize("缓存测试")
        assert len(result) > 0
        # Communicate should only be called once (first call)
        assert mock_module.Communicate.call_count == 1

    @pytest.mark.asyncio
    async def test_synthesize_to_file(self, engine: TTSEngine, mock_edge_tts, tmp_path: Path):
        out = tmp_path / "output.mp3"
        result_path = await engine.synthesize_to_file("写入文件", out)
        assert result_path == out
        assert out.exists()
        assert out.stat().st_size > 0


# ---------------------------------------------------------------------------
# Cache management
# ---------------------------------------------------------------------------
class TestCacheManagement:
    def test_clear_cache(self, engine: TTSEngine, tmp_path: Path):
        # Manually create some fake cache files
        for i in range(3):
            (engine.cache_dir / f"fake{i}.mp3").write_bytes(b"\x00" * 10)
        assert len(list(engine.cache_dir.glob("*.mp3"))) == 3

        count = engine.clear_cache()
        assert count == 3
        assert len(list(engine.cache_dir.glob("*.mp3"))) == 0

    def test_clear_cache_empty(self, engine: TTSEngine):
        count = engine.clear_cache()
        assert count == 0


# ---------------------------------------------------------------------------
# Voice override
# ---------------------------------------------------------------------------
class TestVoiceOverride:
    @pytest.mark.asyncio
    async def test_voice_override(self, engine: TTSEngine, mock_edge_tts):
        mock_module, _ = mock_edge_tts
        await engine.synthesize("测试", voice="zh-CN-YunxiNeural")
        call_kwargs = mock_module.Communicate.call_args
        assert call_kwargs.kwargs.get("voice") == "zh-CN-YunxiNeural" or \
               (len(call_kwargs.args) > 0 and "Yunxi" in str(call_kwargs))
