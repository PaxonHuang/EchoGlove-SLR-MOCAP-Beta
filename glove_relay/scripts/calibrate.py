# -*- coding: utf-8 -*-
"""
Calibration tool for EchoGlove V5 flex sensors.

Collects min/max reference values for 5-point piecewise linear calibration.
Run this before data collection to ensure accurate [0,1] normalization.

Usage
-----
    # Connect to receiver via serial
    python -m scripts.calibrate --port /dev/ttyUSB0

    # Connect to receiver via UDP
    python -m scripts.calibrate --source udp --port 8888

Protocol
--------
1. Open hand (all fingers extended) → 3 seconds → captures MAX values
2. Fist (all fingers curled) → 3 seconds → captures MIN values
3. Per-finger extended → 2 seconds each → captures per-finger MAX
4. Saves calibration to calibration.json
"""
from __future__ import annotations

import argparse
import json
import sys
import time
from pathlib import Path
from typing import Optional

import numpy as np

NUM_FLEX = 5
FINGER_NAMES = ["thumb", "index", "middle", "ring", "pinky"]
CALIBRATION_FRAMES = 300  # 3 seconds at 100Hz


class FlexCalibrator:
    """Collects and stores flex sensor calibration data."""

    def __init__(self) -> None:
        self.min_values = np.ones(NUM_FLEX, dtype=np.float32)
        self.max_values = np.zeros(NUM_FLEX, dtype=np.float32)
        self.per_finger_max = np.zeros(NUM_FLEX, dtype=np.float32)
        self._frame_count = 0

    def update(self, flex_values: list[float]) -> None:
        """Update min/max with new flex readings."""
        arr = np.array(flex_values[:NUM_FLEX], dtype=np.float32)
        self.min_values = np.minimum(self.min_values, arr)
        self.max_values = np.maximum(self.max_values, arr)
        self._frame_count += 1

    def update_finger(self, finger_idx: int, flex_values: list[float]) -> None:
        """Update per-finger max during individual finger calibration."""
        arr = np.array(flex_values[:NUM_FLEX], dtype=np.float32)
        self.per_finger_max[finger_idx] = max(
            self.per_finger_max[finger_idx], arr[finger_idx]
        )

    def compute_5point_calibration(self) -> list[list[list[float]]]:
        """
        Compute 5-point piecewise linear calibration for each finger.

        Returns: calibration_table[finger][point] = [raw, normalized]
        """
        table = []
        for i in range(NUM_FLEX):
            lo = self.min_values[i]
            hi = self.per_finger_max[i] if self.per_finger_max[i] > 0 else self.max_values[i]
            if hi <= lo:
                hi = lo + 1.0  # prevent division by zero

            points = []
            for j in range(5):
                raw = lo + (hi - lo) * j / 4.0
                norm = j / 4.0
                points.append([float(raw), float(norm)])
            table.append(points)
        return table

    def save(self, path: Path) -> None:
        """Save calibration to JSON."""
        data = {
            "version": 5,
            "timestamp": time.time(),
            "num_fingers": NUM_FLEX,
            "finger_names": FINGER_NAMES,
            "min_values": self.min_values.tolist(),
            "max_values": self.max_values.tolist(),
            "per_finger_max": self.per_finger_max.tolist(),
            "calibration_table": self.compute_5point_calibration(),
            "frame_count": self._frame_count,
        }
        path.parent.mkdir(parents=True, exist_ok=True)
        with open(path, "w", encoding="utf-8") as f:
            json.dump(data, f, indent=2)
        print(f"\n✓ Calibration saved to {path}")

    @staticmethod
    def load(path: Path) -> "FlexCalibrator":
        """Load calibration from JSON."""
        with open(path, "r", encoding="utf-8") as f:
            data = json.load(f)
        cal = FlexCalibrator()
        cal.min_values = np.array(data["min_values"], dtype=np.float32)
        cal.max_values = np.array(data["max_values"], dtype=np.float32)
        cal.per_finger_max = np.array(data["per_finger_max"], dtype=np.float32)
        return cal


def read_serial_frame(port, baudrate: int = 115200) -> Optional[list[float]]:
    """Read a single frame from serial. Returns 5 flex values or None."""
    try:
        import serial
        ser = serial.Serial(port, baudrate, timeout=1.0)
        line = ser.readline().decode("utf-8", errors="ignore").strip()
        ser.close()
        if not line:
            return None
        parts = line.split(",")
        if len(parts) >= 5:
            return [float(p) for p in parts[:5]]
    except Exception:
        pass
    return None


def read_udp_frame(host: str, port: int) -> Optional[list[float]]:
    """Read a single frame from UDP. Returns 5 flex values or None."""
    import socket
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.bind((host, port))
        sock.settimeout(2.0)
        data, _ = sock.recvfrom(4096)
        sock.close()
        # Parse V5 protobuf or fallback to raw
        try:
            from proto.glove_data_pb2 import ReceiverPacket
            rp = ReceiverPacket()
            rp.ParseFromString(data)
            if rp.left.flex:
                return list(rp.left.flex)[:5]
        except Exception:
            pass
        # Fallback: assume first 5 floats
        if len(data) >= 20:
            import struct
            return [struct.unpack_from("<f", data, i * 4)[0] for i in range(5)]
    except Exception:
        pass
    return None


def run_calibration(source: str, port, output: Path) -> None:
    """Run the interactive calibration process."""
    cal = FlexCalibrator()

    print("╔══════════════════════════════════════════════════════╗")
    print("║     EchoGlove V5 — Flex Sensor Calibration Tool     ║")
    print("╠══════════════════════════════════════════════════════╣")
    print("║  Step 1: Open hand (all fingers extended) — 3 sec   ║")
    print("║  Step 2: Fist (all fingers curled) — 3 sec          ║")
    print("║  Step 3: Per-finger extended — 2 sec each           ║")
    print("╚══════════════════════════════════════════════════════╝")
    print()

    def get_frame() -> Optional[list[float]]:
        if source == "serial":
            return read_serial_frame(port)
        return read_udp_frame("0.0.0.0", port)

    # Step 1: Open hand
    print("🖐  Open your hand wide (all fingers extended)...")
    input("   Press Enter when ready...")
    print(f"   Collecting {CALIBRATION_FRAMES} frames...")
    for i in range(CALIBRATION_FRAMES):
        frame = get_frame()
        if frame:
            cal.update(frame)
        if (i + 1) % 100 == 0:
            print(f"   {i + 1}/{CALIBRATION_FRAMES} frames")
    print(f"   ✓ Open hand: min={cal.min_values}, max={cal.max_values}")

    # Step 2: Fist
    print("\n✊ Make a fist (all fingers curled)...")
    input("   Press Enter when ready...")
    print(f"   Collecting {CALIBRATION_FRAMES} frames...")
    for i in range(CALIBRATION_FRAMES):
        frame = get_frame()
        if frame:
            cal.update(frame)
        if (i + 1) % 100 == 0:
            print(f"   {i + 1}/{CALIBRATION_FRAMES} frames")
    print(f"   ✓ Fist: min={cal.min_values}, max={cal.max_values}")

    # Step 3: Per-finger
    for fi, name in enumerate(FINGER_NAMES):
        print(f"\n👆 Extend only your {name} finger...")
        input("   Press Enter when ready...")
        per_finger_frames = 200  # 2 seconds
        for i in range(per_finger_frames):
            frame = get_frame()
            if frame:
                cal.update_finger(fi, frame)
            if (i + 1) % 50 == 0:
                print(f"   {i + 1}/{per_finger_frames} frames")
        print(f"   ✓ {name}: max={cal.per_finger_max[fi]:.4f}")

    # Save
    cal.save(output)
    print("\nCalibration complete! Use this file with FlexManager.")
    print(f"  Load: FlexManager::loadCalibration(\"{output}\")")


def main() -> None:
    parser = argparse.ArgumentParser(description="EchoGlove V5 Flex Sensor Calibration")
    parser.add_argument("--source", choices=["serial", "udp"], default="serial")
    parser.add_argument("--port", type=str, default="/dev/ttyUSB0",
                        help="Serial port or UDP port number")
    parser.add_argument("--output", type=str, default="calibration.json",
                        help="Output calibration file")
    args = parser.parse_args()

    port = args.port
    if args.source == "udp":
        try:
            port = int(port)
        except ValueError:
            print("UDP port must be a number")
            sys.exit(1)

    run_calibration(args.source, port, Path(args.output))


if __name__ == "__main__":
    main()
