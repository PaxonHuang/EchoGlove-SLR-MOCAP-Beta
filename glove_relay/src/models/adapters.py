# -*- coding: utf-8 -*-
"""
glove_relay.src.models.adapters — BaseModel adapters for bare ``nn.Module`` models.

The model registry (see :mod:`model_registry`) requires every registered model
to expose the :class:`BaseModel` interface (``forward`` / ``predict`` /
``get_config`` / ``get_model_info``).  The actual V6 L1/L2 architectures are
plain ``torch.nn.Module`` subclasses (``Tier1CNN``, ``GatedBiCrossAttention``)
that do not inherit :class:`BaseModel`; these adapters wrap them without
modifying the model files themselves.

Input conventions (aligned to the 11-dim V6 feature vector — 5 flex + 3 euler
+ 3 gyro per hand):

* **L1 (single hand)**  ``x`` shape ``(batch, 11)``
* **L2 (dual hand)**    ``x`` shape ``(batch, 22)`` = ``[left(11) ‖ right(11)]``
  (a 3D ``(batch, T, 22)`` window is also accepted — the cross-attention model
  averages over the time axis internally)
"""
from __future__ import annotations

from typing import Any

import numpy as np
import torch

from src.models.base_model import BaseModel
from src.models.tier1_cnn import Tier1CNN
from src.models.tier2_cross_attn import GatedBiCrossAttention


def _as_model_input(x: torch.Tensor | np.ndarray, model: torch.nn.Module) -> torch.Tensor:
    """Normalise a numpy array or tensor onto the model's dtype/device."""
    if isinstance(x, np.ndarray):
        x = torch.from_numpy(x)
    if not x.is_floating_point():
        x = x.float()
    return x.to(next(model.parameters()).device)


def _argmax_confidence(logits: torch.Tensor) -> tuple[int, float]:
    """Top-1 class index + softmax confidence for a batch of logits."""
    prob = torch.softmax(logits, dim=-1)
    conf, idx = prob.max(dim=-1)
    return int(idx.item()), float(conf.item())


class Tier1Adapter(BaseModel, torch.nn.Module):
    """BaseModel adapter over :class:`Tier1CNN` (single-hand, 11-dim input)."""

    def __init__(self, input_dim: int = 11, num_classes: int = 46, hidden: int = 64):
        super().__init__()
        self.model = Tier1CNN(input_dim=input_dim, num_classes=num_classes, hidden=hidden)

    def forward(self, x: torch.Tensor) -> torch.Tensor:
        return self.model(x)

    def predict(self, x: torch.Tensor | np.ndarray) -> tuple[int, float]:
        return _argmax_confidence(self.forward(_as_model_input(x, self)))

    def get_config(self) -> dict[str, Any]:
        return {"input_dim": 11, "num_classes": 46, "hidden": 64}

    def get_model_info(self) -> dict[str, Any]:
        return {
            "name": "Tier1CNN",
            "params": sum(p.numel() for p in self.parameters()),
        }


class CrossAttnAdapter(BaseModel, torch.nn.Module):
    """BaseModel adapter over :class:`GatedBiCrossAttention` (dual-hand, 22-dim)."""

    def __init__(self, d_model: int = 32, num_classes: int = 46, input_dim: int = 11):
        super().__init__()
        self.model = GatedBiCrossAttention(d_model=d_model, num_classes=num_classes)
        self._input_dim = input_dim

    def forward(self, x: torch.Tensor) -> torch.Tensor:
        # x: (batch, 22) = [left(11) ‖ right(11)]; split on the last dim so
        # both 2D frames and 3D (batch, T, 22) windows are handled uniformly.
        left = x[..., : self._input_dim]
        right = x[..., self._input_dim : 2 * self._input_dim]
        return self.model(left, right)

    def predict(self, x: torch.Tensor | np.ndarray) -> tuple[int, float]:
        return _argmax_confidence(self.forward(_as_model_input(x, self)))

    def get_config(self) -> dict[str, Any]:
        return {"d_model": 32, "num_classes": 46, "input_dim": self._input_dim}

    def get_model_info(self) -> dict[str, Any]:
        return {
            "name": "GatedBiCrossAttention",
            "params": sum(p.numel() for p in self.parameters()),
        }
