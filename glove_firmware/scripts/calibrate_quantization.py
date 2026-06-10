"""Generate calibration data for INT8 quantization of Tier2 model.

Generates 200 sample pairs (left[11], right[11]) as NumPy .npy files.
These can be used as a representative dataset for TFLite INT8 calibration.

Usage:
    python calibrate_quantization.py
"""
from __future__ import annotations

import os

import numpy as np

INPUT_DIM = 11
NUM_SAMPLES = 200
OUTPUT_DIR = os.path.join(os.path.dirname(__file__), "calibration_data")


def main() -> None:
    os.makedirs(OUTPUT_DIR, exist_ok=True)

    # Generate calibration samples from standard normal distribution
    # In production, replace with real sensor data captures
    left_data = np.random.randn(NUM_SAMPLES, INPUT_DIM).astype(np.float32)
    right_data = np.random.randn(NUM_SAMPLES, INPUT_DIM).astype(np.float32)

    np.save(os.path.join(OUTPUT_DIR, "calib_left.npy"), left_data)
    np.save(os.path.join(OUTPUT_DIR, "calib_right.npy"), right_data)

    print(f"Generated {NUM_SAMPLES} calibration samples in {OUTPUT_DIR}/")
    print(f"  calib_left.npy  shape={left_data.shape}  dtype={left_data.dtype}")
    print(f"  calib_right.npy shape={right_data.shape} dtype={right_data.dtype}")
    print("\nTo use in export_model.py, replace representative_dataset()")
    print("with loads from these .npy files.")


if __name__ == "__main__":
    main()
