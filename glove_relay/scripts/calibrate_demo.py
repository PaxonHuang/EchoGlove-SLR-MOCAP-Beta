#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
glove_relay/scripts/calibrate_demo.py — Interactive dynamic-range + per-letter
template capture for the ASL demo.

Guides the user through 8 holds (~3 s each) over the live $EG USB CDC stream:
  1. OPEN hand  (B-pose, all fingers straight) → per-channel RAW MIN
  2. FIST       (A-pose, all fingers bent)     → per-channel RAW MAX
  3..8. Letters A / B / I / L / W / Y           → per-letter normalized template

Then:
  - auto-detects per-channel polarity (raw rising vs falling when bent)
  - prints the inter-template euclidean distance matrix as a sanity check
  - saves everything to glove_relay/configs/demo_calibration.json
  - (demo_server loads this file at startup → live classification switches to
     per-user templates automatically)

NO firmware reflash — reads the same $EG ASCII stream demo_server uses.

Run:
    /home/paxon/anaconda3/envs/pytorch_env/bin/python scripts/calibrate_demo.py
    /home/paxon/anaconda3/envs/pytorch_env/bin/python scripts/calibrate_demo.py --port /dev/ttyACM0 --hold 3
"""
from __future__ import annotations

import argparse
import math
import statistics
import sys
import time
from pathlib import Path
from typing import List, Optional, Tuple

_HERE = Path(__file__).resolve().parent
_RELAY_ROOT = _HERE.parent
sys.path.insert(0, str(_RELAY_ROOT))

from src.inference.asl_classifier import (  # noqa: E402
    ASLClassifier, ASL_LETTERS, ASL_TEMPLATES, DEFAULT_CALIBRATION_PATH,
    ACTIVE_CHANNELS, ACTIVE_DIM,
)

try:
    import serial  # type: ignore
except ImportError:
    sys.stderr.write("pyserial not installed. `pip install pyserial`\n")
    raise

ASCII_PREFIX = "$EG,"


# ── Serial stream reader ───────────────────────────────────────────────────
class EGStream:
    """Blocking reader for ``$EG,f0,f1,f2,f3,f4,tick\\n`` lines."""

    def __init__(self, port: str, baud: int) -> None:
        self.ser = serial.Serial(port, baud, timeout=1.0)

    def close(self) -> None:
        try:
            self.ser.close()
        except Exception:
            pass

    def _readline(self) -> Optional[List[float]]:
        # pyserial readline blocks up to `timeout`; returns b'' on timeout.
        raw = self.ser.readline()
        if not raw:
            return None
        try:
            s = raw.decode("ascii", errors="replace").strip()
        except Exception:
            return None
        if not s.startswith(ASCII_PREFIX):
            return None
        parts = s[len(ASCII_PREFIX):].split(",")
        if len(parts) < 5:
            return None
        try:
            return [float(p) for p in parts[:5]]
        except ValueError:
            return None

    def capture(self, seconds: float) -> Tuple[List[List[float]], int]:
        """Collect raw 5-vectors for `seconds` seconds. Returns (samples, n)."""
        samples: List[List[float]] = []
        end = time.time() + seconds
        while time.time() < end:
            v = self._readline()
            if v is not None:
                samples.append(v)
        return samples, len(samples)


# ── Guided capture ─────────────────────────────────────────────────────────
def _countdown(prompt: str, seconds: int) -> None:
    print(f"\n>>> {prompt}")
    for i in range(seconds, 0, -1):
        print(f"    starting in {i}…", end="\r", flush=True)
        time.sleep(1)
    print(" " * 40, end="\r")
    print("    ● CAPTURING — hold steady")


def _prompt(label: str, hold: int, stream: EGStream
            ) -> Tuple[List[List[float]], int]:
    _countdown(label, 3)
    samples, n = stream.capture(hold)
    print(f"    captured {n} frames")
    if n < 10:
        print("    ⚠ low frame count — check serial / hold still")
    return samples, n


def _channel_mean(samples: List[List[float]]) -> List[float]:
    return [statistics.mean(s[i] for s in samples) for i in range(5)]


def _channel_min(samples: List[List[float]]) -> List[float]:
    """Per-channel MINIMUM across samples. The true straight/raw-floor is the
    floor of the captured window, not the mean — a finger only hits full
    extension for part of the hold, and mean averages that back toward middle,
    under-reporting the real range (this was the first capture's failure mode).
    """
    return [min(s[i] for s in samples) for i in range(5)]


def _channel_max(samples: List[List[float]]) -> List[float]:
    return [max(s[i] for s in samples) for i in range(5)]


GUIDE = {
    "OPEN": "OPEN hand (B-pose): spread ALL fingers fully straight, palm forward",
    "FIST": "FIST (A-pose): curl ALL fingers into a tight fist, thumb outside",
    "A": "Letter A: fist with thumb tucked along the SIDE of the index finger",
    "B": "Letter B: flat palm, 4 fingers straight together, thumb folded across palm",
    "I": "Letter I: fist with ONLY the pinky extended straight up",
    "L": "Letter L: index up + thumb out sideways, 90° angle, others curled",
}
# NOTE: W and Y are NOT captured — the ring-finger channel (ch3/GPIO4) is a
# confirmed hardware fault on this glove, and W/Y need a working ring channel
# to separate from B/A respectively. The 4 active letters A/B/I/L separate
# cleanly on the 4 working channels (thumb/index/middle/pinky).


def main() -> int:
    p = argparse.ArgumentParser(description="ASL demo dynamic-range + template capture")
    p.add_argument("--port", default="/dev/ttyACM0")
    p.add_argument("--baud", type=int, default=115200)
    p.add_argument("--hold", type=float, default=3.0,
                   help="seconds to capture per pose (default 3)")
    p.add_argument("--out", default=str(DEFAULT_CALIBRATION_PATH),
                   help="output JSON path")
    args = p.parse_args()

    clf = ASLClassifier()
    n_letters = len(ASL_LETTERS)          # 4 (A/B/I/L)
    n_steps = 2 + n_letters               # OPEN + FIST + 4 letters = 6
    print(f"=== EchoGlove ASL calibration capture ===")
    print(f"Serial: {args.port} @ {args.baud}   Hold: {args.hold}s   Out: {args.out}")
    print(f"Active letters: {' '.join(ASL_LETTERS)} on channels "
          f"{[names[i] for i in ACTIVE_CHANNELS]} (ring ch3 disabled — hw fault).")
    print("Each pose: 3s prep countdown, then hold steady while capturing.")
    print("Range step uses per-channel MIN/MAX (not mean) — hit the FULL extreme.")
    stream = EGStream(args.port, args.baud)
    names = ["thumb", "index", "middle", "ring ", "pinky"]
    # raw-count swing threshold (on the /4095 scale 0.05 ≈ 205 counts). Only
    # the ACTIVE channels are checked — ch3 (ring) is a confirmed hardware
    # fault and will never swing, so it must not block capture.
    MIN_SWING = 0.05
    try:
        # ── Phase 1+2: dynamic range (open → min, fist → max) ────────────
        # Loop OPEN+FIST until every ACTIVE channel shows a usable swing.
        for attempt in range(1, 4):
            open_s, _ = _prompt(GUIDE["OPEN"] + f"  [step 1/{n_steps}: range MIN]"
                                + (f"  (attempt {attempt})" if attempt > 1 else ""),
                                int(args.hold), stream)
            fist_s, _ = _prompt(GUIDE["FIST"] + f"  [step 2/{n_steps}: range MAX]"
                                + (f"  (attempt {attempt})" if attempt > 1 else ""),
                                int(args.hold), stream)
            open_lo = _channel_min(open_s); open_hi = _channel_max(open_s)
            fist_lo = _channel_min(fist_s); fist_hi = _channel_max(fist_s)
            open_mean = _channel_mean(open_s); fist_mean = _channel_mean(fist_s)
            polarity = ASLClassifier.detect_polarity(open_mean, fist_mean)
            range_min: List[float] = [0.0] * 5
            range_max: List[float] = [0.0] * 5
            for i in range(5):
                if polarity[i] == 1:            # raw rises when bent
                    range_min[i] = open_lo[i]    # straight floor
                    range_max[i] = fist_hi[i]    # bent ceiling
                else:                            # raw drops when bent
                    range_min[i] = fist_lo[i]
                    range_max[i] = open_hi[i]
            clf.set_range(range_min, range_max, polarity)
            print("\n    range (raw/4095 scale; raw count = ×4095):")
            for i in range(5):
                pol = "↑rises" if polarity[i] == 1 else "↓drops"
                sw = range_max[i] - range_min[i]
                tag = "  [DISABLED hw-fault]" if i not in ACTIVE_CHANNELS else ""
                print(f"      ch{i} {names[i]}: min={range_min[i]:.3f} "
                      f"max={range_max[i]:.3f}  swing={sw:.3f} (~{int(sw*4095)} cnt)  {pol}{tag}")
            # Only check the ACTIVE channels; ch3 (ring) is ignored.
            small = [i for i in ACTIVE_CHANNELS
                     if abs(range_max[i] - range_min[i]) < MIN_SWING]
            if not small:
                print("    ✓ all ACTIVE channels have usable swing — proceeding to letters.")
                break
            print(f"    ✗ active channels {small} swing < {MIN_SWING} — those fingers "
                  "did not reach a true extreme.")
            print("      Redo OPEN+FIST. TIP: spread fingers WIDE and press them "
                  "BACK for OPEN; squeeze a TIGHT fist wrapping the thumb for FIST.")
            if attempt < 3:
                print("      (will retry automatically)\n")
            else:
                print("    ⚠ still small after 3 attempts — continuing anyway "
                      "(check sensor coupling for those channels).")

        # ── Phase 3..n: per-letter templates ─────────────────────────────
        spec_proj = {L: [ASL_TEMPLATES[L][i] for i in ACTIVE_CHANNELS]
                     for L in ASL_LETTERS}
        for idx, L in enumerate(ASL_LETTERS, start=3):
            samples, _ = _prompt(GUIDE[L] + f"  [step {idx}/{n_steps}: letter {L}]",
                                 int(args.hold), stream)
            tmpl = clf.capture_letter(L, samples)
            spec = spec_proj[L]
            print(f"    {L} captured = [{', '.join(f'{x:.2f}' for x in tmpl)}]"
                  f"   spec = [{', '.join(f'{x:.2f}' for x in spec)}]"
                  f"  (ch {list(ACTIVE_CHANNELS)})")
            d = math.dist(tmpl, spec)
            print(f"        drift from spec: {d:.2f}")

        clf.use_captured(True)
    finally:
        stream.close()

    # ── Sanity: distance matrix over captured templates ──────────────────
    print("\n=== inter-template euclidean distance (captured) ===")
    print(clf.distance_matrix())

    # ── Quality gate: refuse to save a collapsed calibration ────────────
    # If two different letters captured to nearly-identical templates (min
    # inter-class distance too small), the classifier cannot tell them apart
    # and saving this file would make demo_server silently misclassify. The
    # first capture attempt hit this (min dist = 0.004, A==B). Refuse + tell
    # the user which pair collapsed so they can redo those letters crisply.
    # Templates are stored projected to ACTIVE_DIM, so distances here already
    # exclude the broken ring channel.
    tmpls = clf.captured_templates
    letters = [L for L in ASL_LETTERS if L in tmpls]
    pairs = [(a, b, math.dist(tmpls[a], tmpls[b]))
             for i, a in enumerate(letters) for b in letters[i + 1:]]
    if pairs:
        pairs.sort(key=lambda p: p[2])
        mn_a, mn_b, mn_d = pairs[0]
        # Theoretical min for A/B/I/L on 4 good channels is 0.923 (A vs L);
        # require at least 0.30 in practice (handles capture noise + pose drift).
        MIN_ACCEPTABLE = 0.30
        print(f"\nmin inter-class distance: {mn_d:.3f} ({mn_a} vs {mn_b})")
        if mn_d < MIN_ACCEPTABLE:
            print(f"\n✗ REFUSING to save — {mn_a} and {mn_b} captured too close "
                  f"(dist {mn_d:.3f} < {MIN_ACCEPTABLE}).")
            print("  Those two letters normalized to nearly the same vector, so")
            print("  at least one finger did not reach a distinct position.")
            print(f"  Re-run and hold {mn_a} and {mn_b} with maximum contrast on")
            print("  the thumb/index/middle/pinky channels. Nothing was saved.")
            return 1
        print(f"✓ min distance {mn_d:.3f} ≥ {MIN_ACCEPTABLE} — calibration usable.")

    # ── Save ─────────────────────────────────────────────────────────────
    out = Path(args.out)
    clf.save(out)
    print(f"\n✓ calibration saved → {out}")
    print("  Start demo_server.py (it auto-loads this) → browser will show letters.")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except KeyboardInterrupt:
        print("\naborted.")
        raise SystemExit(130)
