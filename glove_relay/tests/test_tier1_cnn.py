import pytest
import torch
from src.models.tier1_cnn import Tier1CNN

@pytest.fixture
def model():
    m = Tier1CNN(input_dim=11, num_classes=46)
    m.eval()
    return m

def test_output_shape(model):
    out = model(torch.randn(1, 11))
    assert out.shape == (1, 46)

def test_window_input(model):
    out = model(torch.randn(1, 30, 11))
    assert out.shape == (1, 46)

def test_param_count(model):
    count = sum(p.numel() for p in model.parameters())
    assert 10000 < count < 30000

def test_output_probabilities(model):
    out = torch.softmax(model(torch.randn(1, 11)), dim=1)
    assert torch.allclose(out.sum(), torch.tensor(1.0), atol=1e-5)
