# -*- coding: utf-8 -*-
"""Tests for the ASL 6-letter template classifier + dynamic-range calibration."""
import math
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from src.inference.asl_classifier import (  # noqa: E402
    ASLClassifier,
    ASL_TEMPLATES,
    ASL_LETTERS,
)


# ── Theoretical templates (from the ASL spec) ──────────────────────────────
def test_spec_templates_present():
    # Every letter in the spec must have a 5-dim template in 0..1.
    for L in ASL_LETTERS:
        t = ASL_TEMPLATES[L]
        assert len(t) == 5
        assert all(0.0 <= x <= 1.0 for x in t)


def test_each_template_classifies_as_itself():
    c = ASLClassifier()
    for L, t in ASL_TEMPLATES.items():
        r = c.classify_normalized(list(t))
        assert r["letter"] == L, f"template {L} matched {r['letter']}"
        assert r["confidence"] > 0.9


def test_min_interclass_distance_is_large():
    # NOTE: the ASL spec §3.2 distance table is WRONG. It claims min = 0.80
    # (B vs W) and A-B = 1.64, but the real values are min = 0.559 (I vs Y)
    # and A-B = 1.875. The closest pair I/Y differ mainly on thumb + pinky.
    # The reject threshold (0.34) must sit below the I/Y midpoint (~0.28) so
    # borderline readings still classify.
    pairs = []
    for i, a in enumerate(ASL_LETTERS):
        for b in ASL_LETTERS[i + 1:]:
            pairs.append((a, b, math.dist(ASL_TEMPLATES[a], ASL_TEMPLATES[b])))
    pairs.sort(key=lambda p: p[2])
    nearest = pairs[0]
    assert nearest[0:2] == ("I", "Y") or nearest[0:2] == ("Y", "I"), \
        f"expected I/Y to be closest, got {nearest}"
    assert nearest[2] > 0.5, f"min inter-class distance too small: {nearest[2]:.3f}"
    assert nearest[2] < 0.6   # guards the spec-table correction above


def test_unknown_rejected():
    # A vector far from all templates → no match (id 0, confidence low).
    c = ASLClassifier(reject_threshold=0.45)
    r = c.classify_normalized([0.5, 0.5, 0.5, 0.5, 0.5])  # dead-centre, equidistant-ish
    assert r["letter"] is None
    assert r["gesture_id"] == 0


def test_noisy_template_still_matches():
    # Add noise ~0.1 (well under the 0.80 class gap) → still matches.
    c = ASLClassifier()
    t = list(ASL_TEMPLATES["L"])
    noisy = [max(0.0, min(1.0, x + 0.08)) for x in t]
    r = c.classify_normalized(noisy)
    assert r["letter"] == "L"


# ── Range normalization + polarity ─────────────────────────────────────────
def test_range_normalization_positive_polarity():
    # Channel rises when bent: open raw=0.4, fist raw=0.85 (post /4095 scale).
    c = ASLClassifier()
    c.set_range(min=[0.4] * 5, max=[0.85] * 5, polarity=[1] * 5)
    # A fully-bent raw (0.85) → 1.0; fully-straight (0.4) → 0.0
    assert abs(c.normalize([0.85] * 5)[0] - 1.0) < 1e-6
    assert abs(c.normalize([0.4] * 5)[0] - 0.0) < 1e-6


def test_range_normalization_inverted_polarity():
    # A channel where raw DROPS when bent (some flex sensors / wiring): invert.
    c = ASLClassifier()
    c.set_range(min=[0.3] * 5, max=[0.9] * 5, polarity=[-1] * 5)
    # raw=0.3 (the "min", but min was captured at OPEN) with inverted polarity
    # → bent maps to 1.0, straight to 0.0. Straight = max raw (0.9) → 0.0.
    assert abs(c.normalize([0.9] * 5)[0] - 0.0) < 1e-6
    assert abs(c.normalize([0.3] * 5)[0] - 1.0) < 1e-6


def test_classify_uses_normalization():
    # Feed raw /4095-scale values; with range set, A-pose raw maps to A template.
    c = ASLClassifier()
    # Suppose full swing is raw 0.40..0.85 for every channel (positive polarity).
    c.set_range(min=[0.40] * 5, max=[0.85] * 5, polarity=[1] * 5)
    # A = [0.30, 0.95, 1.00, 1.00, 0.95] normalized → raw = min + n*(max-min)
    span = 0.85 - 0.40
    a_raw = [0.40 + 0.30 * span, 0.40 + 0.95 * span, 0.40 + 1.00 * span,
             0.40 + 1.00 * span, 0.40 + 0.95 * span]
    r = c.classify_raw(a_raw)
    assert r["letter"] == "A"
    assert r["confidence"] > 0.9


# ── Persistence ────────────────────────────────────────────────────────────
def test_save_load_roundtrip():
    c1 = ASLClassifier()
    c1.set_range(min=[0.4, 0.42, 0.41, 0.43, 0.4],
                 max=[0.86, 0.88, 0.87, 0.89, 0.85],
                 polarity=[1, 1, 1, -1, 1])
    c1.set_captured_templates({"A": ASL_TEMPLATES["A"], "B": ASL_TEMPLATES["B"]})
    c1.use_captured(True)

    with tempfile.TemporaryDirectory() as d:
        path = Path(d) / "cal.json"
        c1.save(path)
        c2 = ASLClassifier()
        assert c2.load(path)
        # Construct a raw vector that normalizes exactly to captured template A,
        # respecting channel-3 inverted polarity (norm 1.0 → raw = min).
        a_norm = ASL_TEMPLATES["A"]
        raw = [0.4 + a_norm[0] * 0.46, 0.42 + a_norm[1] * 0.46, 0.41 + a_norm[2] * 0.46,
               0.43,                          # inverted: norm 1.00 → raw = min
               0.4 + a_norm[4] * 0.45]
        assert c2.normalize(raw) == c1.normalize(raw)
        r = c2.classify_raw(raw)
        assert r["letter"] == "A"
        assert r["confidence"] > 0.9


def test_load_missing_returns_false():
    c = ASLClassifier()
    assert not c.load(Path("/nonexistent/demo_calibration.json"))


def test_capture_letter_averages_samples():
    c = ASLClassifier()
    c.set_range(min=[0.4] * 5, max=[0.85] * 5, polarity=[1] * 5)
    # Two raw samples representing an A-pose → captured template ≈ A normalized.
    span = 0.45
    a_raw = lambda: [0.40 + 0.30 * span, 0.40 + 0.95 * span, 0.40 + 1.00 * span,
                     0.40 + 1.00 * span, 0.40 + 0.95 * span]
    c.capture_letter("A", [a_raw(), a_raw(), a_raw()])
    cap = c.captured_templates["A"]
    assert abs(cap[0] - 0.30) < 1e-3
