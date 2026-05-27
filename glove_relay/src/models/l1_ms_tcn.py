# -*- coding: utf-8 -*-
from __future__ import annotations

"""
glove_relay.src.models.l1_ms_tcn — Multi-Scale Temporal Convolutional Network (L1).

Architecture (≈ 12 K parameters)
================================
- Input: ``(B, T, 21)`` — short temporal window of sensor data.
- Three dilated Conv1d stages (dilation 1, 2, 4) with residual connections.
- Global Average Pooling → FC → 46-class logits.

MS-TCN captures multi-scale temporal patterns efficiently and is highly
suitable for edge deployment due to its small footprint.

Training
--------
::

    python -m src.models.l1_ms_tcn \\
        --data_dir data/train \\
        --epochs 100 \\
        --batch_size 64 \\
        --lr 0.001 \\
        --num_classes 46
"""


import argparse
from typing import Any

import torch
import torch.nn as nn
import torch.nn.functional as F

from src.models.base_model import BaseModel
from src.utils.logger import get_logger

logger = get_logger(__name__)


# ---------------------------------------------------------------------------
# Sub-modules
# ---------------------------------------------------------------------------
class _ResidualBlock(nn.Module):
    """
    Single dilated convolutional residual block.

    Parameters
    ----------
    in_ch : int
        Input channels.
    out_ch : int
        Output channels.
    dilation : int
        Dilation rate for the Conv1d.
    kernel_size : int
        Kernel size (default 3).
    """

    def __init__(self, in_ch: int, out_ch: int, dilation: int, kernel_size: int = 3) -> None:
        super().__init__()
        padding = (kernel_size - 1) * dilation // 2
        self.conv = nn.Conv1d(in_ch, out_ch, kernel_size, padding=padding, dilation=dilation)
        self.bn = nn.BatchNorm1d(out_ch)
        self.relu = nn.ReLU(inplace=True)

        # Shortcut projection if channel count changes
        self.downsample: nn.Module | None = None
        if in_ch != out_ch:
            self.downsample = nn.Conv1d(in_ch, out_ch, kernel_size=1)

    def forward(self, x: torch.Tensor) -> torch.Tensor:
        """Forward with residual connection."""
        identity = x
        out = self.relu(self.bn(self.conv(x)))
        if self.downsample is not None:
            identity = self.downsample(identity)
        return out + identity


# ---------------------------------------------------------------------------
# Main model
# ---------------------------------------------------------------------------
class MSTCNModel(BaseModel, nn.Module):
    """
    Multi-Scale TCN for gesture classification from short temporal windows.

    Parameters
    ----------
    input_dim : int
        Number of sensor features per frame (default 21).
    num_classes : int
        Number of gesture classes (default 46).
    base_channels : int
        Base number of convolutional channels (default 32).
    num_stages : int
        Number of dilated residual stages (default 3).
    """

    def __init__(
        self,
        input_dim: int = 21,
        num_classes: int = 46,
        base_channels: int = 32,
        num_stages: int = 3,
        **_kwargs: Any,
    ) -> None:
        super().__init__()

        self.input_dim = input_dim
        self.num_classes = num_classes

        # --- Input projection: (B, T, 21) → (B, base_channels, T) ---
        self.input_proj = nn.Conv1d(input_dim, base_channels, kernel_size=1)

        # --- Dilated residual stages ---
        self.stages = nn.ModuleList()
        for i in range(num_stages):
            dilation = 2 ** i  # 1, 2, 4
            out_ch = base_channels * (2 ** min(i, 1))  # 32, 64, 64
            self.stages.append(_ResidualBlock(base_channels if i == 0 else out_ch // (2 ** min(i, 1)) if i > 1 else base_channels * (2 ** (i - 1)), out_ch, dilation))

        # Recalculate stage channels properly
        self.stages = nn.ModuleList()
        in_ch = base_channels
        for i in range(num_stages):
            dilation = 2 ** i
            out_ch = base_channels * 2 if i >= 1 else base_channels
            self.stages.append(_ResidualBlock(in_ch, out_ch, dilation))
            in_ch = out_ch

        # --- Classification head ---
        self.gap = nn.AdaptiveAvgPool1d(1)
        self.dropout = nn.Dropout(0.3)
        self.fc = nn.Linear(in_ch, num_classes)

    def forward(self, x: torch.Tensor) -> torch.Tensor:
        """
        Forward pass.

        Parameters
        ----------
        x : torch.Tensor
            ``(B, T, 21)`` or ``(B, 21)`` for single-frame input.

        Returns
        -------
        torch.Tensor
            ``(B, num_classes)`` — raw logits.
        """
        # Handle single-frame input: (B, 21) → (B, 1, 21) → (B, T, 21)
        if x.dim() == 2:
            x = x.unsqueeze(1)

        # (B, T, 21) → (B, 21, T)
        x = x.permute(0, 2, 1)

        x = self.input_proj(x)  # (B, base_ch, T)
        for stage in self.stages:
            x = stage(x)

        x = self.gap(x).squeeze(-1)  # (B, C)
        x = self.dropout(x)
        return self.fc(x)

    @torch.no_grad()
    def predict(self, x: torch.Tensor) -> tuple[int, float]:
        """
        Inference helper.

        Returns
        -------
        tuple[int, float]
            ``(gesture_id, confidence)`` for a single sample.
        """
        self.eval()
        logits = self.forward(x)
        if logits.dim() > 1 and logits.size(0) == 1:
            logits = logits.squeeze(0)
        probs = F.softmax(logits, dim=-1)
        confidence, idx = probs.max(dim=-1)
        return int(idx.item()), float(confidence.item())

    def get_config(self) -> dict[str, Any]:
        return {
            "input_dim": self.input_dim,
            "num_classes": self.num_classes,
            "architecture": "ms_tcn_v1",
            "base_channels": getattr(self, "_base_channels", 32),
        }

    def get_model_info(self) -> dict[str, Any]:
        return {
            "name": "ms_tcn_v1",
            "params": sum(p.numel() for p in self.parameters()),
            "trainable_params": sum(p.numel() for p in self.parameters() if p.requires_grad),
        }


# ---------------------------------------------------------------------------
# Training script
# ---------------------------------------------------------------------------
def _train(args: argparse.Namespace) -> None:
    """Training loop (skeleton — replace dummy data with real dataset)."""
    from torch.utils.data import DataLoader, TensorDataset

    logger.info("Training MS-TCN model …")
    device = torch.device("cuda" if torch.cuda.is_available() and not args.cpu else "cpu")
    model = MSTCNModel(input_dim=21, num_classes=args.num_classes).to(device)

    logger.warning("Using dummy dataset — replace with real data for training")
    dummy_x = torch.randn(500, 10, 21)  # (B, T=10, 21)
    dummy_y = torch.randint(0, args.num_classes, (500,))
    dataset = TensorDataset(dummy_x, dummy_y)
    train_loader = DataLoader(dataset, batch_size=args.batch_size, shuffle=True)

    criterion = nn.CrossEntropyLoss()
    optimizer = torch.optim.Adam(model.parameters(), lr=args.lr, weight_decay=1e-4)
    scheduler = torch.optim.lr_scheduler.CosineAnnealingLR(optimizer, T_max=args.epochs)

    best_acc = 0.0
    for epoch in range(1, args.epochs + 1):
        model.train()
        total_loss = 0.0
        correct = 0
        total = 0

        for xb, yb in train_loader:
            xb, yb = xb.to(device), yb.to(device)
            optimizer.zero_grad()
            logits = model(xb)
            loss = criterion(logits, yb)
            loss.backward()
            optimizer.step()
            total_loss += loss.item() * xb.size(0)
            correct += (logits.argmax(dim=-1) == yb).sum().item()
            total += xb.size(0)

        scheduler.step()
        avg_loss = total_loss / max(total, 1)
        acc = correct / max(total, 1)

        if acc > best_acc:
            best_acc = acc
            torch.save(model.state_dict(), "checkpoints/l1_ms_tcn_v1.pt")
            logger.info("Epoch %d/%d — loss: %.4f  acc: %.4f  ★ saved", epoch, args.epochs, avg_loss, acc)
        elif epoch % 10 == 0:
            logger.info("Epoch %d/%d — loss: %.4f  acc: %.4f", epoch, args.epochs, avg_loss, acc)

    logger.info("Training complete. Best accuracy: %.4f", best_acc)


def main() -> None:
    """CLI entry point for training."""
    parser = argparse.ArgumentParser(description="Train MS-TCN model")
    parser.add_argument("--data_dir", type=str, default="data/train", help="Training data directory")
    parser.add_argument("--epochs", type=int, default=100, help="Number of training epochs")
    parser.add_argument("--batch_size", type=int, default=64, help="Batch size")
    parser.add_argument("--lr", type=float, default=0.001, help="Learning rate")
    parser.add_argument("--num_classes", type=int, default=46, help="Number of gesture classes")
    parser.add_argument("--cpu", action="store_true", help="Force CPU training")
    _train(parser.parse_args())


if __name__ == "__main__":
    main()
