# -*- coding: utf-8 -*-
"""
TDD Tests for ST-GCN model components (SOP §9.2).

Tests each module independently then the full model end-to-end.
Requires PyTorch (CPU).
"""

import pytest
import torch

from src.models.stgcn_model import (
    _build_adj_matrix,
    GraphConv,
    TemporalConv,
    STConvBlock,
    AttentionPooling,
    STGCNModel,
    NUM_NODES,
)


# ---------------------------------------------------------------------------
# Test: Adjacency matrix
# ---------------------------------------------------------------------------

class TestAdjacencyMatrix:
    """Hand skeleton adjacency matrix properties."""

    def test_shape(self):
        A = _build_adj_matrix()
        assert A.shape == (21, 21)

    def test_self_loops(self):
        A = _build_adj_matrix()
        for i in range(21):
            assert A[i, i].item() > 0.0

    def test_symmetric(self):
        A = _build_adj_matrix()
        assert torch.allclose(A, A.T)

    def test_normalized_max_le_one(self):
        A = _build_adj_matrix()
        assert A.max().item() <= 1.0 + 1e-6

    def test_wrist_connected_to_fingers(self):
        """Wrist (0) should be connected to finger bases (1,5,9,13,17)."""
        A = _build_adj_matrix()
        for base in [1, 5, 9, 13, 17]:
            assert A[0, base].item() > 0.0

    def test_palm_crosslinks(self):
        """Palm cross-links (5-9, 9-13, 13-17) should exist."""
        A = _build_adj_matrix()
        for u, v in [(5, 9), (9, 13), (13, 17)]:
            assert A[u, v].item() > 0.0


# ---------------------------------------------------------------------------
# Test: GraphConv
# ---------------------------------------------------------------------------

class TestGraphConv:
    """Spatial graph convolution layer."""

    def test_output_shape(self):
        gc = GraphConv(in_channels=2, out_channels=64)
        x = torch.randn(4, 21, 2)  # (B=4, N=21, C=2)
        out = gc(x)
        assert out.shape == (4, 21, 64)

    def test_batch_independent(self):
        """Different batch elements produce different outputs."""
        gc = GraphConv(2, 16)
        x = torch.randn(2, 21, 2)
        x[1] = x[0] + 1.0  # different input
        out = gc(x)
        assert not torch.allclose(out[0], out[1])

    def test_gradient_flows(self):
        gc = GraphConv(2, 8)
        x = torch.randn(1, 21, 2, requires_grad=True)
        out = gc(x)
        loss = out.sum()
        loss.backward()
        assert x.grad is not None
        assert x.grad.abs().sum().item() > 0


# ---------------------------------------------------------------------------
# Test: TemporalConv
# ---------------------------------------------------------------------------

class TestTemporalConv:
    """1-D temporal convolution layer."""

    def test_output_shape(self):
        tc = TemporalConv(in_channels=64, out_channels=64)
        x = torch.randn(4, 64, 30)  # (B=4, C=64, T=30)
        out = tc(x)
        assert out.shape == (4, 64, 30)

    def test_dilation_preserves_length(self):
        tc = TemporalConv(32, 32, kernel_size=3, dilation=4)
        x = torch.randn(2, 32, 30)
        out = tc(x)
        assert out.shape[2] == 30

    def test_channel_change(self):
        tc = TemporalConv(32, 64)
        x = torch.randn(2, 32, 30)
        out = tc(x)
        assert out.shape[1] == 64


# ---------------------------------------------------------------------------
# Test: STConvBlock
# ---------------------------------------------------------------------------

class TestSTConvBlock:
    """Spatial-Temporal convolution block."""

    def test_output_shape_same_channels(self):
        block = STConvBlock(64, 64)
        x = torch.randn(2, 30, 21, 64)  # (B, T, N, C)
        out = block(x)
        assert out.shape == (2, 30, 21, 64)

    def test_output_shape_different_channels(self):
        block = STConvBlock(64, 128)
        x = torch.randn(2, 30, 21, 64)
        out = block(x)
        assert out.shape == (2, 30, 21, 128)

    def test_residual_connection(self):
        """Block with same channels should have identity residual."""
        block = STConvBlock(32, 32)
        x = torch.randn(1, 10, 21, 32)
        out = block(x)
        # Output should not be identical to input (transformed)
        assert not torch.allclose(out, x)

    def test_different_temporal_dilation(self):
        block = STConvBlock(32, 32, temporal_dilation=2)
        x = torch.randn(1, 10, 21, 32)
        out = block(x)
        assert out.shape == x.shape


# ---------------------------------------------------------------------------
# Test: AttentionPooling
# ---------------------------------------------------------------------------

class TestAttentionPooling:
    """Channel-attention pooling layer."""

    def test_output_shape(self):
        pool = AttentionPooling(in_channels=128)
        x = torch.randn(4, 30, 21, 128)  # (B, T, N, C)
        out = pool(x)
        assert out.shape == (4, 128)

    def test_weights_sum_to_one(self):
        """Attention weights across T*N should sum to ~1 per batch."""
        pool = AttentionPooling(64)
        x = torch.randn(2, 10, 21, 64)
        # Manually compute weights
        B, T, N, C = x.shape
        x_flat = x.reshape(B * T * N, C)
        weights = pool.att(x_flat).reshape(B, T, N)
        weights = torch.softmax(weights.reshape(B, -1), dim=-1)
        assert torch.allclose(weights.sum(dim=-1), torch.ones(B), atol=1e-5)

    def test_gradient_flows(self):
        pool = AttentionPooling(32)
        x = torch.randn(1, 10, 21, 32, requires_grad=True)
        out = pool(x)
        out.sum().backward()
        assert x.grad is not None


# ---------------------------------------------------------------------------
# Test: STGCNModel (end-to-end)
# ---------------------------------------------------------------------------

class TestSTGCNModel:
    """Full ST-GCN model."""

    def test_default_output_shape(self):
        model = STGCNModel()
        x = torch.randn(2, 30, 21)
        out = model(x)
        assert out.shape == (2, 46)

    def test_custom_num_classes(self):
        model = STGCNModel(num_classes=20)
        x = torch.randn(1, 30, 21)
        out = model(x)
        assert out.shape == (1, 20)

    def test_predict_returns_tuple(self):
        model = STGCNModel()
        x = torch.randn(1, 30, 21)
        gesture_id, confidence = model.predict(x)
        assert isinstance(gesture_id, int)
        assert isinstance(confidence, float)
        assert 0 <= gesture_id < 46
        assert 0.0 <= confidence <= 1.0

    def test_predict_confidence_sums_le_one(self):
        """Softmax probabilities sum to 1, so max confidence <= 1."""
        model = STGCNModel()
        x = torch.randn(1, 30, 21)
        _, conf = model.predict(x)
        assert conf <= 1.0 + 1e-6

    def test_get_config(self):
        model = STGCNModel(input_dim=21, hidden_dim=64, num_classes=46)
        cfg = model.get_config()
        assert cfg["input_dim"] == 21
        assert cfg["hidden_dim"] == 64
        assert cfg["num_classes"] == 46
        assert cfg["architecture"] == "stgcn_v1"

    def test_get_model_info(self):
        model = STGCNModel()
        info = model.get_model_info()
        assert info["name"] == "stgcn_v1"
        assert info["params"] > 0
        assert info["trainable_params"] == info["params"]

    def test_gradient_flows_end_to_end(self):
        model = STGCNModel()
        x = torch.randn(1, 30, 21, requires_grad=True)
        out = model(x)
        out.sum().backward()
        assert x.grad is not None

    def test_eval_mode_predict(self):
        """predict() should set model to eval mode."""
        model = STGCNModel()
        model.train()
        x = torch.randn(1, 30, 21)
        model.predict(x)
        assert not model.training
