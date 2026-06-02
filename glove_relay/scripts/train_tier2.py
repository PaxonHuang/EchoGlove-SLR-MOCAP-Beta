"""Train Tier2 Gated Bi-CrossAttention model on dual-hand CSV data."""
from __future__ import annotations
import argparse
import sys
import os
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import pandas as pd
import torch
import torch.nn as nn
from src.models.tier2_cross_attn import GatedBiCrossAttention


def load_dual_data(csv_path: str):
    df = pd.read_csv(csv_path)
    hand_cols = [f"flex{i}" for i in range(5)] + ["euler_x", "euler_y", "euler_z", "gyro_x", "gyro_y", "gyro_z"]
    left = torch.tensor(df[hand_cols].values, dtype=torch.float32)
    right_cols = [f"r_{c}" for c in hand_cols]
    right = torch.tensor(df[right_cols].values, dtype=torch.float32)
    y = torch.tensor(df["gesture_id"].values, dtype=torch.long)
    return left, right, y


def train(csv_path: str, output_path: str, epochs: int = 100):
    left, right, y = load_dual_data(csv_path)
    model = GatedBiCrossAttention(d_model=32, num_classes=46)
    optimizer = torch.optim.Adam(model.parameters(), lr=1e-3)
    criterion = nn.CrossEntropyLoss()
    for epoch in range(epochs):
        out = model(left, right)
        loss = criterion(out, y)
        optimizer.zero_grad()
        loss.backward()
        optimizer.step()
        if (epoch + 1) % 10 == 0:
            print(f"Epoch {epoch+1}/{epochs} loss={loss.item():.4f}")
    model.eval()
    traced = torch.jit.trace(model, (torch.randn(1, 11), torch.randn(1, 11)))
    traced.save(output_path)
    print(f"Saved to {output_path}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--data", required=True)
    parser.add_argument("--output", default="models/tier2_model.pt")
    parser.add_argument("--epochs", type=int, default=100)
    args = parser.parse_args()
    train(args.data, args.output, args.epochs)
