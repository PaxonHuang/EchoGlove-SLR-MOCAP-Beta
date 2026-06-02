"""Train Tier1 CNN model on single-hand CSV data."""
from __future__ import annotations
import argparse
import sys
import os
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import pandas as pd
import torch
import torch.nn as nn
from torch.utils.data import DataLoader, TensorDataset
from src.models.tier1_cnn import Tier1CNN


def load_data(csv_path: str):
    df = pd.read_csv(csv_path)
    feature_cols = [f"flex{i}" for i in range(5)] + ["euler_x", "euler_y", "euler_z", "gyro_x", "gyro_y", "gyro_z"]
    X = torch.tensor(df[feature_cols].values, dtype=torch.float32)
    y = torch.tensor(df["gesture_id"].values, dtype=torch.long)
    return X, y


def train(csv_path: str, output_path: str, epochs: int = 100, batch_size: int = 32):
    X, y = load_data(csv_path)
    dataset = TensorDataset(X, y)
    loader = DataLoader(dataset, batch_size=batch_size, shuffle=True)
    model = Tier1CNN(input_dim=11, num_classes=46)
    optimizer = torch.optim.Adam(model.parameters(), lr=1e-3)
    criterion = nn.CrossEntropyLoss()
    for epoch in range(epochs):
        total_loss = 0
        for batch_x, batch_y in loader:
            out = model(batch_x)
            loss = criterion(out, batch_y)
            optimizer.zero_grad()
            loss.backward()
            optimizer.step()
            total_loss += loss.item()
        if (epoch + 1) % 10 == 0:
            print(f"Epoch {epoch+1}/{epochs} loss={total_loss/len(loader):.4f}")
    model.eval()
    traced = torch.jit.trace(model, torch.randn(1, 11))
    traced.save(output_path)
    print(f"Saved to {output_path}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--data", required=True)
    parser.add_argument("--output", default="models/tier1_model.pt")
    parser.add_argument("--epochs", type=int, default=100)
    args = parser.parse_args()
    train(args.data, args.output, args.epochs)
