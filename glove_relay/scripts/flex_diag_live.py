#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
glove_relay/scripts/flex_diag_live.py — Real-time per-channel raw ADC diagnostic.

Prints a live, single-line table of the 5 flex channels (raw ADC 0..4095 AND
the firmware-normalized raw/4095 value that demo_server sees) so you can watch
each channel respond as you bend each finger. This bypasses ALL calibration /
template logic and directly answers: "does the ADC voltage change when a
finger moves?"

Usage:
    /home/paxon/anaconda3/envs/pytorch_env/bin/python scripts/flex_diag_live.py
    /home/paxon/anaconda3/envs/pytorch_env/bin/python scripts/flex_diag_live.py --port /dev/ttyACM0

The firmware line is:  $EG,f0,f1,f2,f3,f4,tick
where f0..f4 = raw/4095 (FlexManager uncalibrated fallback).
We multiply back by 4095 to show the true 12-bit ADC count alongside.

Hold each finger steady and watch its channel: it should swing clearly
(hundreds of ADC counts) between straight and bent. If it barely moves,
that channel's sensor/wiring is the problem.
"""
from __future__ import annotations

import argparse
import sys
import time
from typing import List, Optional

try:
    import serial  # type: ignore
except ImportError:
    sys.stderr.write("pyserial not installed. `pip install pyserial`\n")
    raise

ASCII_PREFIX = "$EG,"
NAMES = ["thumb", "index", "middle", "ring ", "pinky"]


def parse(line: str) -> Optional[List[float]]:
    s = line.strip()
    if not s.startswith(ASCII_PREFIX):
        return None
    parts = s[len(ASCII_PREFIX):].split(",")
    if len(parts) < 5:
        return None
    try:
        return [float(p) for p in parts[:5]]
    except ValueError:
        return None


def main() -> int:
    p = argparse.ArgumentParser(description="Real-time flex ADC diagnostic")
    p.add_argument("--port", default="/dev/ttyACM0")
    p.add_argument("--baud", type=int, default=115200)
    p.add_argument("--secs", type=float, default=60.0,
                   help="seconds to run the live view (default 60)")
    args = p.parse_args()

    ser = serial.Serial(args.port, args.baud, timeout=0.2)
    print(f"=== flex live ADC diagnostic ({args.secs:.0f}s) on {args.port} ===")
    print("Channels: ch0=thumb ch1=index ch2=middle ch3=ring ch4=pinky")
    print("Bend each finger and watch its RAW count swing (straight vs bent).")
    print("A healthy channel swings HUNDREDS of counts. A stuck channel "
          "barely moves.\n")
    print("        ch0 thumb   ch1 index   ch2 middle  ch3 ring    ch4 pinky")
    print("        raw  /4095  raw  /4095  raw  /4095  raw  /4095  raw  /4095")

    # Track per-channel min/max *observed during this session* so the user can
    # see the live swing envelope grow as they move.
    obs_min = [4095] * 5
    obs_max = [0] * 5
    t0 = time.time()
    try:
        while time.time() - t0 < args.secs:
            raw = ser.readline()
            if not raw:
                continue
            f = parse(raw.decode("ascii", errors="replace"))
            if f is None:
                continue
            counts = [int(round(v * 4095)) for v in f]
            for i, c in enumerate(counts):
                if c < obs_min[i]:
                    obs_min[i] = c
                if c > obs_max[i]:
                    obs_max[i] = c
            line = "  ".join(f"{c:4d}/ {f[i]:.3f}" for i, c in enumerate(counts))
            swing = "  swing[" + ",".join(
                f"{obs_max[i]-obs_min[i]:4d}" for i in range(5)) + "]"
            sys.stdout.write("\r  " + line + swing + "   ")
            sys.stdout.flush()
    except KeyboardInterrupt:
        pass
    finally:
        ser.close()

    print("\n\n=== observed swing this session (max-min of raw 12-bit count) ===")
    for i in range(5):
        sw = obs_max[i] - obs_min[i]
        flag = "  ← STUCK (sensor/wiring?)" if sw < 50 else "  OK"
        print(f"  ch{i} {NAMES[i]}: min={obs_min[i]:4d} max={obs_max[i]:4d} "
              f"swing={sw:4d}{flag}")
    print("\nIf a channel is STUCK, that finger's flex sensor is not changing "
          "the ADC voltage — check sensor coupling / wiring for that channel.")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except KeyboardInterrupt:
        print("\nbye.")
        raise SystemExit(130)
