import pytest
import torch
from src.models.tier2_cross_attn import GatedBiCrossAttention

@pytest.fixture
def model():
    return GatedBiCrossAttention(d_model=32, num_classes=46)

def test_output_shape(model):
    out = model(torch.randn(1, 11), torch.randn(1, 11))
    assert out.shape == (1, 46)

def test_param_count(model):
    count = sum(p.numel() for p in model.parameters())
    assert 5000 < count < 30000

def test_window_input(model):
    out = model(torch.randn(1, 30, 11), torch.randn(1, 30, 11))
    assert out.shape == (1, 46)
