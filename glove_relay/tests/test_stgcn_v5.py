import pytest
import torch
from src.models.stgcn_model import STGCNModel

@pytest.fixture
def model():
    return STGCNModel(num_nodes=42, in_features=28, num_classes=46)

def test_output_shape(model):
    out = model(torch.randn(1, 30, 42, 28))
    assert out.shape == (1, 46)

def test_12node_variant():
    m = STGCNModel(num_nodes=12, in_features=11, num_classes=46)
    out = m(torch.randn(1, 30, 12, 11))
    assert out.shape == (1, 46)

def test_edge_count_42node(model):
    assert model.num_edges == 88

def test_edge_count_12node():
    m = STGCNModel(num_nodes=12, in_features=11, num_classes=46)
    assert m.num_edges == 22
