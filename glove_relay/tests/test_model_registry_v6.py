# -*- coding: utf-8 -*-
"""
Regression tests for the V6 model-registry fixes (B1 + B3).

Covered:
  * B1 — the class map resolves to real modules via the BaseModel adapters
    (previously pointed at non-existent ``l1_cnn_attention`` / ``l1_ms_tcn``).
  * B3 — the active pipeline is 11-dim (5 flex + 3 euler + 3 gyro) and the
    active L2 is the dual-hand ``GatedBiCrossAttention`` (not ST-GCN).
  * warm-up + ``predict()`` work for both L1 (11-dim) and L2 (22-dim concat,
    including the 3D ``(T, 22)`` window path used by the legacy UDP server).
"""
from __future__ import annotations

import numpy as np
import pytest

from src.models.model_registry import ModelRegistry
from src.utils.config import get_config, load_model_config


@pytest.fixture
def registry() -> ModelRegistry:
    """A fresh registry loaded from the (fixed) YAML config."""
    r = ModelRegistry()
    r.load_from_config()
    return r


def test_class_map_resolves_to_adapters(registry: ModelRegistry) -> None:
    assert type(registry.l1_model).__name__ == "Tier1Adapter"
    assert type(registry.l2_model).__name__ == "CrossAttnAdapter"


def test_active_models_from_config(registry: ModelRegistry) -> None:
    assert registry.active_l1_name == "cnn_attention_v2"
    assert registry.active_l2_name == "gated_cross_attn_v1"


def test_config_input_dim_is_11() -> None:
    mc = load_model_config()
    assert mc["l1_models"][0]["input_dim"] == 11
    l2 = {m["name"]: m for m in mc["l2_models"]}
    assert l2[mc["active_l2_model"]]["input_dim"] == 22  # [left(11) ‖ right(11)]


def test_l1_predict_numpy(registry: ModelRegistry) -> None:
    gid, conf = registry.l1_model.predict(np.random.randn(1, 11).astype(np.float32))
    assert isinstance(gid, int)
    assert isinstance(conf, float)
    assert 0.0 <= conf <= 1.0


def test_l2_predict_22dim(registry: ModelRegistry) -> None:
    gid, conf = registry.l2_model.predict(np.random.randn(1, 22).astype(np.float32))
    assert isinstance(gid, int)
    assert isinstance(conf, float)


def test_l2_predict_3d_window(registry: ModelRegistry) -> None:
    # (1, T, 22) — the shape the legacy UDP server builds per frame window
    gid, conf = registry.l2_model.predict(np.random.randn(1, 30, 22).astype(np.float32))
    assert isinstance(gid, int)
    assert isinstance(conf, float)


def test_hot_switch_l2(registry: ModelRegistry) -> None:
    assert registry.switch("l2", "gated_cross_attn_v1") is True
    assert registry.active_l2_name == "gated_cross_attn_v1"


def test_mock_defaults_to_false() -> None:
    # B7 — non-mock (real hardware) is the default; RELAY_MOCK=1 opts back in.
    assert get_config().mock.enabled is False
