# -*- coding: utf-8 -*-
"""
glove_relay.scripts.data_collector — V5 dual-hand data collection tool.

Supports data sources:
  * **Serial** — connects to the receiver ESP32 via USB serial.
  * **UDP** — receives V5 ReceiverPacket datagrams from the receiver.

Features
--------
- Real-time ASCII waveform display.
- Record labelled gestures and save as ``.csv`` files (V5 dual-hand format).
- On-the-fly data augmentation (time shift, Gaussian noise, time masking).
- Supports both left and right hand recording.

CSV Format (V5)
---------------
    tick_id, timestamp, hand_id,
    flex_0..flex_4, euler_0..euler_2, gyro_0..gyro_2,
    l1_gesture_id, l1_confidence, status

Usage
-----
    # Record from UDP (default port 8888)
    python -m scripts.data_collector --source udp --output data/recorded

    # Record from serial
    python -m scripts.data_collector --source serial --port /dev/ttyUSB0 --output data/recorded

    # Record with labels
    python -m scripts.data_collector --source udp --labels data/gesture_labels.json
"""
from __future__ import annotations

import argparse
import csv
import json
import sys
import time
from pathlib import Path
from typing import Any, Dict, List, Optional

import numpy as np

NUM_FLEX = 5
IMU_DIM = 6
SINGLE_HAND_DIM = NUM_FLEX + IMU_DIM  # 11
CSV_HEADER = [
    "tick_id", "timestamp", "hand_id",
    "flex_0", "flex_1", "flex_2", "flex_3", "flex_4",
    "euler_0", "euler_1", "euler_2",
    "gyro_0", "gyro_1", "gyro_2",
    "l1_gesture_id", "l1_confidence", "status",
]


# ---------------------------------------------------------------------------
# Data augmentation
# ---------------------------------------------------------------------------
def augment_time_shift(data: np.ndarray, max_shift: int = 3) -> np.ndarray:
    shift = np.random.randint(-max_shift, max_shift + 1)
    if shift == 0:
        return data.copy()
    result = np.zeros_like(data)
    if shift > 0:
        result[shift:] = data[:-shift]
    else:
        result[:shift] = data[-shift:]
    return result


def augment_gaussian_noise(data: np.ndarray, sigma: float = 0.02) -> np.ndarray:
    noise = np.random.normal(0, sigma, size=data.shape).astype(np.float32)
    return data + noise


def augment_time_masking(data: np.ndarray, max_mask_ratio: float = 0.15) -> np.ndarray:
    result = data.copy()
    T = data.shape[0]
    mask_len = max(1, int(T * max_mask_ratio * np.random.random()))
    start = np.random.randint(0, T - mask_len + 1)
    result[start:start + mask_len] = 0.0
    return result


def apply_augmentation(data: np.ndarray, num_augmented: int = 3) -> List[np.ndarray]:
    augmented = []
    augmenters = [augment_time_shift, augment_gaussian_noise, augment_time_masking]
    for _ in range(num_augmented):
        sample = data.copy()
        n_ops = np.random.randint(1, 3)
        chosen = np.random.choice(len(augmenters), size=n_ops, replace=False)
        for idx in chosen:
            sample = augmenters[idx](sample)
        augmented.append(sample)
    return augmented


# ---------------------------------------------------------------------------
# ASCII waveform
# ---------------------------------------------------------------------------
def display_waveform(frame: np.ndarray, title: str = "Live Sensor Data") -> None:
    cols = 60
    min_v, max_v = frame.min(), frame.max()
    rng = max_v - min_v if max_v != min_v else 1.0

    # Flex sensors (0–4)
    flex = frame[:NUM_FLEX]
    print(f"\n{'=' * cols}  {title}")
    for i, v in enumerate(flex):
        bar_len = int((v - min_v) / rng * (cols - 20))
        bar = "█" * bar_len
        print(f"  F{i} [{v:7.4f}] {bar}")

    # IMU (5–10)
    imu = frame[NUM_FLEX:NUM_FLEX + IMU_DIM]
    mid = cols // 2
    for i, v in enumerate(imu):
        pos = int(mid + (v / (rng or 1)) * (mid - 5))
        pos = max(1, min(cols - 2, pos))
        line = [" "] * cols
        line[mid] = "│"
        line[pos] = "●"
        labels = ["E0", "E1", "E2", "G0", "G1", "G2"]
        print(f"  {labels[i]}  {''.join(line)}")
    print(f"{'=' * cols}")


# ---------------------------------------------------------------------------
# Recording session
# ---------------------------------------------------------------------------
class RecordingSession:
    """Manages a V5 dual-hand gesture recording session."""

    def __init__(
        self,
        output_dir: Path,
        window_size: int = 30,
        label_file: Optional[Path] = None,
    ) -> None:
        self.output_dir = output_dir
        self.window_size = window_size
        self.labels: Dict[int, str] = {}
        self._buffer: List[dict] = []

        output_dir.mkdir(parents=True, exist_ok=True)

        if label_file and label_file.exists():
            with open(label_file, "r", encoding="utf-8") as fh:
                for entry in json.load(fh):
                    self.labels[entry["id"]] = entry.get("name_cn", entry.get("name", f"gesture_{entry['id']}"))

    def add_frame(self, frame: dict) -> None:
        """Append a V5 frame dict to the buffer."""
        self._buffer.append(frame.copy())
        if len(self._buffer) > self.window_size:
            self._buffer.pop(0)

    def save_recording(self, gesture_id: int, hand_id: int = 0) -> Path:
        """Save the buffer as a CSV file with optional augmentation."""
        label_name = self.labels.get(gesture_id, f"unknown_{gesture_id}")
        hand_tag = "L" if hand_id == 0 else "R"
        timestamp = int(time.time())

        # Save original CSV
        filename = f"gesture_{gesture_id:03d}_{label_name}_{hand_tag}_{timestamp}.csv"
        filepath = self.output_dir / filename
        self._write_csv(filepath)

        # Save augmented copies
        for i in range(3):
            aug_name = f"gesture_{gesture_id:03d}_{label_name}_{hand_tag}_aug{i}_{timestamp}.csv"
            aug_path = self.output_dir / aug_name
            self._write_csv_augmented(aug_path)

        self._buffer.clear()
        return filepath

    def _write_csv(self, path: Path) -> None:
        with open(path, "w", newline="", encoding="utf-8") as f:
            writer = csv.writer(f)
            writer.writerow(CSV_HEADER)
            for frame in self._buffer:
                row = self._frame_to_row(frame)
                writer.writerow(row)
        print(f"  ✓ Saved: {path}  ({len(self._buffer)} frames)")

    def _write_csv_augmented(self, path: Path) -> None:
        frames = []
        for frame in self._buffer:
            flex = np.array(frame.get("flex", [0.0] * NUM_FLEX), dtype=np.float32)
            imu = np.array(frame.get("imu", [0.0] * IMU_DIM), dtype=np.float32)
            combined = np.concatenate([flex, imu])
            augmented = apply_augmentation(combined.reshape(1, -1), 1)[0]
            frames.append({
                "tick_id": frame.get("tick_id", 0),
                "timestamp": frame.get("timestamp", 0),
                "hand_id": frame.get("hand_id", 0),
                "flex": augmented[:NUM_FLEX].tolist(),
                "imu": augmented[NUM_FLEX:].tolist(),
                "l1_gesture_id": frame.get("l1_gesture_id", -1),
                "l1_confidence": frame.get("l1_confidence", 0.0),
                "status": frame.get("status", ""),
            })
        with open(path, "w", newline="", encoding="utf-8") as f:
            writer = csv.writer(f)
            writer.writerow(CSV_HEADER)
            for frame in frames:
                writer.writerow(self._frame_to_row(frame))
        print(f"  ✓ Augmented: {path}")

    @staticmethod
    def _frame_to_row(frame: dict) -> list:
        flex = frame.get("flex", [0.0] * NUM_FLEX)
        imu = frame.get("imu", [0.0] * IMU_DIM)
        return [
            frame.get("tick_id", 0),
            frame.get("timestamp", 0.0),
            frame.get("hand_id", 0),
            *flex[:NUM_FLEX],
            *imu[:IMU_DIM],
            frame.get("l1_gesture_id", -1),
            frame.get("l1_confidence", 0.0),
            frame.get("status", ""),
        ]


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------
def main() -> None:
    parser = argparse.ArgumentParser(
        description="EchoGlove V5 Dual-Hand Data Collection Tool",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument("--source", choices=["serial", "udp"], default="udp")
    parser.add_argument("--port", type=str, default="/dev/ttyUSB0",
                        help="Serial port or UDP port number")
    parser.add_argument("--output", type=str, default="data/recorded")
    parser.add_argument("--window", type=int, default=30)
    parser.add_argument("--labels", type=str, default="data/gesture_labels.json")
    parser.add_argument("--visualize", action="store_true")
    parser.add_argument("--hand", choices=["left", "right", "both"], default="both")

    args = parser.parse_args()

    session = RecordingSession(
        output_dir=Path(args.output),
        window_size=args.window,
        label_file=Path(args.labels) if Path(args.labels).exists() else None,
    )

    print("╔══════════════════════════════════════════════════════╗")
    print("║   EchoGlove V5 — Dual-Hand Data Collection Tool     ║")
    print("╠══════════════════════════════════════════════════════╣")
    print(f"║  Source: {args.source:<43s}  ║")
    print(f"║  Output: {args.output:<43s}  ║")
    print(f"║  Window: {args.window} frames{' ' * 36}  ║")
    print(f"║  Hand:   {args.hand:<43s}  ║")
    print("╠══════════════════════════════════════════════════════╣")
    print("║  Commands:                                          ║")
    print("║    r <id>  — Record gesture with given label ID     ║")
    print("║    q       — Quit                                   ║")
    print("╚══════════════════════════════════════════════════════╝")
    print()

    if session.labels:
        print(f"Available labels: {session.labels}")
    print("\nReady. Enter 'r <id>' to record, 'q' to quit.")

    # Serial mode
    if args.source == "serial":
        try:
            import serial
            ser = serial.Serial(args.port, 115200, timeout=1.0)
            print(f"Connected to {args.port}")

            while True:
                try:
                    cmd = input("\n> ").strip()
                except (EOFError, KeyboardInterrupt):
                    break

                if cmd.lower() == "q":
                    break
                if cmd.lower().startswith("r "):
                    try:
                        gid = int(cmd.split()[1])
                    except (ValueError, IndexError):
                        print("  Usage: r <gesture_id>")
                        continue

                    print(f"  Recording gesture {gid} ({session.labels.get(gid, '?')})...")
                    for i in range(args.window):
                        line = ser.readline().decode("utf-8", errors="ignore").strip()
                        if line:
                            parts = line.split(",")
                            if len(parts) >= SINGLE_HAND_DIM:
                                frame = {
                                    "tick_id": i,
                                    "timestamp": time.time(),
                                    "hand_id": 0,
                                    "flex": [float(p) for p in parts[:NUM_FLEX]],
                                    "imu": [float(p) for p in parts[NUM_FLEX:SINGLE_HAND_DIM]],
                                    "l1_gesture_id": gid,
                                    "l1_confidence": 1.0,
                                    "status": "recorded",
                                }
                                session.add_frame(frame)
                                if args.visualize:
                                    combined = np.array(frame["flex"] + frame["imu"])
                                    display_waveform(combined, f"Frame {i + 1}/{args.window}")

                    session.save_recording(gid)
            ser.close()
        except ImportError:
            print("pyserial not installed. Install with: pip install pyserial")
        except Exception as e:
            print(f"Serial error: {e}")
    else:
        print(f"UDP mode not yet implemented in this version. Use serial mode.")

    print("\nData collection session ended.")


if __name__ == "__main__":
    main()
