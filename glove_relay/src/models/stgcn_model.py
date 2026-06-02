"""Hierarchical ST-GCN: 12-node (Tier1/2) and 42-node (Tier3)."""
from __future__ import annotations
import torch
import torch.nn as nn


def build_42node_edges() -> list[tuple[int, int]]:
    edges = []
    for hand_offset in [0, 21]:
        for f in range(5):
            prev = hand_offset
            for j in range(4):
                node = hand_offset + 1 + f * 4 + j
                edges.append((prev, node))
                prev = node
    left_tips = [4, 8, 12, 16, 20]
    right_tips = [25, 29, 33, 37, 41]
    for lt, rt in zip(left_tips, right_tips):
        edges.append((lt, rt))
    edges.append((0, 21))  # wrist-to-wrist bridge
    for i in range(42):
        edges.append((i, i))
    return edges


def build_12node_edges() -> list[tuple[int, int]]:
    edges = []
    for i in range(1, 6):
        edges.append((0, i))
    for i in range(7, 12):
        edges.append((6, i))
    for i in range(1, 6):
        edges.append((i, i + 5))
    for i in range(7):  # self-loops on wrist + left-hand nodes
        edges.append((i, i))
    return edges


class GraphConvolution(nn.Module):
    def __init__(self, in_features: int, out_features: int, num_nodes: int, edges: list):
        super().__init__()
        adj = torch.zeros(num_nodes, num_nodes)
        for a, b in edges:
            adj[a, b] = 1.0
        deg = adj.sum(dim=1, keepdim=True).clamp(min=1)
        adj = adj / deg
        self.register_buffer("adj", adj)
        self.linear = nn.Linear(in_features, out_features)

    def forward(self, x: torch.Tensor) -> torch.Tensor:
        if x.dim() == 4:
            B, T, N, C = x.shape
            x = x.reshape(B * T, N, C)
            x = torch.matmul(self.adj, x)
            x = self.linear(x)
            x = x.reshape(B, T, N, -1)
        else:
            x = torch.matmul(self.adj, x)
            x = self.linear(x)
        return x


class STGCNModel(nn.Module):
    def __init__(self, num_nodes: int, in_features: int, num_classes: int, hidden: int = 64):
        super().__init__()
        self.num_nodes = num_nodes
        edges = build_42node_edges() if num_nodes == 42 else build_12node_edges()
        self.num_edges = len(edges)
        self.spatial = nn.Sequential(
            GraphConvolution(in_features, hidden, num_nodes, edges), nn.ReLU(),
            GraphConvolution(hidden, hidden, num_nodes, edges), nn.ReLU(),
        )
        self.temporal = nn.Sequential(
            nn.Conv1d(hidden, hidden, kernel_size=3, padding=1), nn.ReLU(),
            nn.Conv1d(hidden, hidden, kernel_size=3, padding=1), nn.ReLU(),
        )
        self.classifier = nn.Sequential(
            nn.Linear(hidden * num_nodes, 128), nn.ReLU(),
            nn.Linear(128, num_classes),
        )

    def forward(self, x: torch.Tensor) -> torch.Tensor:
        x = self.spatial(x)
        B, T, N, H = x.shape
        x = x.permute(0, 2, 3, 1).reshape(B * N, H, T)
        x = self.temporal(x).mean(dim=2)
        x = x.reshape(B, N * H)
        return self.classifier(x)
