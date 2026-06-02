"""Gated Bi-CrossAttention for dual-hand feature fusion."""
from __future__ import annotations
import torch
import torch.nn as nn


class GatedBiCrossAttention(nn.Module):
    def __init__(self, d_model: int = 32, num_classes: int = 46):
        super().__init__()
        self.embed = nn.Linear(11, d_model)
        self.cross_attn = nn.MultiheadAttention(d_model, num_heads=4, batch_first=True)
        self.gate_proj = nn.Linear(d_model * 2, d_model)
        self.classifier = nn.Sequential(
            nn.Linear(d_model * 2, d_model),
            nn.ReLU(),
            nn.Linear(d_model, num_classes),
        )

    def forward(self, left: torch.Tensor, right: torch.Tensor) -> torch.Tensor:
        if left.dim() == 3:
            left = left.mean(dim=1)
            right = right.mean(dim=1)
        e_l = self.embed(left).unsqueeze(1)
        e_r = self.embed(right).unsqueeze(1)
        attn_l, _ = self.cross_attn(e_l, e_r, e_r)
        attn_r, _ = self.cross_attn(e_r, e_l, e_l)
        attn_l, attn_r = attn_l.squeeze(1), attn_r.squeeze(1)
        e_l, e_r = e_l.squeeze(1), e_r.squeeze(1)
        gate_l = torch.sigmoid(self.gate_proj(torch.cat([e_l, attn_l], dim=1)))
        gate_r = torch.sigmoid(self.gate_proj(torch.cat([e_r, attn_r], dim=1)))
        fused_l = gate_l * attn_l + (1 - gate_l) * e_l
        fused_r = gate_r * attn_r + (1 - gate_r) * e_r
        return self.classifier(torch.cat([fused_l, fused_r], dim=1))
