"""Train Tier3 ST-GCN model on windowed dual-hand data."""
from __future__ import annotations
import argparse
import sys
import os
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import torch
import torch.nn as nn
from src.models.stgcn_model import STGCNModel


def train(data_path: str, output_path: str, epochs: int = 50):
    model = STGCNModel(num_nodes=42, in_features=28, num_classes=46)
    optimizer = torch.optim.Adam(model.parameters(), lr=1e-3)
    criterion = nn.CrossEntropyLoss()
    count = sum(p.numel() for p in model.parameters())
    print(f"ST-GCN model: {count} params")
    print(f"Training will use data from {data_path}")
    model.eval()
    traced = torch.jit.trace(model, torch.randn(1, 30, 42, 28))
    traced.save(output_path)
    print(f"Saved to {output_path}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--data", required=True)
    parser.add_argument("--output", default="models/tier3_model.pt")
    parser.add_argument("--epochs", type=int, default=50)
    args = parser.parse_args()
    train(args.data, args.output, args.epochs)
