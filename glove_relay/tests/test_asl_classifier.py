# -*- coding: utf-8 -*-
"""Tests for the ASL 4-letter (A/B/I/L) template classifier on 4 working
channels (thumb/index/middle/pinky — the ring channel ch3 is a confirmed
hardware fault on this glove and is masked out of all matching)."""
import math
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from src.inference.asl_classifier import (  # noqa: E402
    ASLClassifier,
    ASL_TEMPLATES,
    ASL_LETTERS,
    ACTIVE_LETTERS,
    ACTIVE_CHANNELS,
    ACTIVE_DIM,
    CONF_SPAN,
)


# ── Active set + channel mask ──────────────────────────────────────────────
def test_active_letters_are_four():
    # The broken-ring demo uses exactly A/B/I/L on channels thumb/index/middle/pinky.
    assert ACTIVE_LETTERS == ("A", "B", "I", "L")
    assert ASL_LETTERS == ACTIVE_LETTERS  # back-compat alias


def test_active_channels_drop_ring():
    # ch3 (ring, GPIO4) is the broken channel — it must NOT be in ACTIVE_CHANNELS.
    assert ACTIVE_CHANNELS == (0, 1, 2, 4)
    assert 3 not in ACTIVE_CHANNELS
    assert ACTIVE_DIM == 4


# ── Theoretical templates ──────────────────────────────────────────────────
def test_spec_templates_present():
    # Every ACTIVE letter has a full 5-D template (reference); W/Y also retained.
    for L in ACTIVE_LETTERS:
        t = ASL_TEMPLATES[L]
        assert len(t) == 5
        assert all(0.0 <= x <= 1.0 for x in t)
    # W/Y retained for reference but not in the active matching set.
    assert "W" in ASL_TEMPLATES and "Y" in ASL_TEMPLATES


def test_each_active_template_classifies_as_itself():
    c = ASLClassifier()
    for L in ACTIVE_LETTERS:
        r = c.classify_normalized(list(ASL_TEMPLATES[L]))  # full 5-D input
        assert r["letter"] == L, f"template {L} matched {r['letter']}"
        assert r["confidence"] > 0.95


def test_w_y_not_in_active_pool():
    # W/Y are NOT matched as themselves (they need the broken ring channel to
    # separate cleanly). They either collapse to an active letter or — better —
    # get rejected (gap > threshold) so they produce 无手势, never a wrong W/Y.
    c = ASLClassifier()
    for L in ("W", "Y"):
        r = c.classify_normalized(list(ASL_TEMPLATES[L]))
        assert r["letter"] != L, f"{L} must not self-classify"
        assert r["letter"] in ACTIVE_LETTERS or r["letter"] is None


def test_min_interclass_distance_is_large_on_4_channels():
    # On the 4 active channels (drop ring), the ACTIVE_LETTERS min inter-class
    # euclidean distance is 0.923 (A vs L) — dropping the broken ring channel
    # IMPROVES separability for these 4 letters. Reject threshold sits ~1/3 of it.
    proj = {L: [ASL_TEMPLATES[L][i] for i in ACTIVE_CHANNELS] for L in ACTIVE_LETTERS}
    pairs = []
    for i, a in enumerate(ACTIVE_LETTERS):
        for b in ACTIVE_LETTERS[i + 1:]:
            pairs.append((a, b, math.dist(proj[a], proj[b])))
    pairs.sort(key=lambda p: p[2])
    nearest = pairs[0]
    assert nearest[2] > 0.9, f"min inter-class distance too small: {nearest[2]:.3f}"
    assert abs(CONF_SPAN - nearest[2]) < 1e-2  # SPAN ≈ min class distance


def test_unknown_rejected():
    # A vector far from all templates → no match (id 0, low confidence).
    c = ASLClassifier(reject_threshold=0.45)
    r = c.classify_normalized([0.5, 0.5, 0.5, 0.5, 0.5])  # dead-centre 5-D
    assert r["letter"] is None
    assert r["gesture_id"] == 0


def test_noisy_template_still_matches():
    # Add noise ~0.1 (well under the 0.923 class gap) → still matches.
    c = ASLClassifier()
    t = list(ASL_TEMPLATES["L"])
    noisy = [max(0.0, min(1.0, x + 0.08)) for x in t]
    r = c.classify_normalized(noisy)
    assert r["letter"] == "L"


# ── Ring channel (ch3) is ignored ──────────────────────────────────────────
def test_ring_channel_does_not_affect_classification():
    # Two frames identical on the 4 good channels but differing wildly on the
    # broken ring channel classify identically, because ring (ch3) is dropped.
    c = ASLClassifier()
    base = list(ASL_TEMPLATES["L"])           # full 5-D
    hacked = list(base); hacked[3] = 1.0 - hacked[3]  # flip ring value
    r1 = c.classify_normalized(base)
    r2 = c.classify_normalized(hacked)
    assert r1["letter"] == r2["letter"] == "L"
    assert abs(r1["distance"] - r2["distance"]) < 1e-9


def test_projected_4dim_input_matches_5dim_input():
    # Giving a 4-D projected vector must give the same result as the full 5-D.
    c = ASLClassifier()
    full = list(ASL_TEMPLATES["A"])
    proj = [full[i] for i in ACTIVE_CHANNELS]
    r1 = c.classify_normalized(full)
    r2 = c.classify_normalized(proj)
    assert r1["letter"] == r2["letter"]
    assert abs(r1["distance"] - r2["distance"]) < 1e-9


# ── Range normalization + polarity ─────────────────────────────────────────
def test_range_normalization_positive_polarity():
    c = ASLClassifier()
    c.set_range(min=[0.4] * 5, max=[0.85] * 5, polarity=[1] * 5)
    # normalize projects to ACTIVE_CHANNELS → length ACTIVE_DIM
    n = c.normalize([0.85] * 5)
    assert len(n) == ACTIVE_DIM
    assert abs(n[0] - 1.0) < 1e-6
    assert abs(c.normalize([0.4] * 5)[0] - 0.0) < 1e-6


def test_range_normalization_inverted_polarity():
    c = ASLClassifier()
    c.set_range(min=[0.3, 0.3, 0.3, 0.3, 0.3], max=[0.9, 0.9, 0.9, 0.9, 0.9],
                polarity=[-1, -1, -1, -1, -1])
    assert abs(c.normalize([0.9] * 5)[0] - 0.0) < 1e-6
    assert abs(c.normalize([0.3] * 5)[0] - 1.0) < 1e-6


def test_classify_uses_normalization():
    c = ASLClassifier()
    # 4-channel swing raw 0.40..0.85, positive polarity on all 5 (rng/polarity
    # still full 5-D; ch3 entry ignored).
    c.set_range(min=[0.40] * 5, max=[0.85] * 5, polarity=[1] * 5)
    span = 0.85 - 0.40
    a_norm = ASL_TEMPLATES["A"]  # full 5-D [0.30,0.95,1.00,1.00,0.95]
    a_raw = [0.40 + a_norm[i] * span for i in range(5)]
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
        # A raw vector normalizing to A on the 4 good channels must match A.
        a_norm = ASL_TEMPLATES["A"]
        raw = [0.4 + a_norm[0] * 0.46, 0.42 + a_norm[1] * 0.46,
               0.41 + a_norm[2] * 0.46,
               0.43,  # ch3 (ring) — broken/ignored; raw value irrelevant
               0.4 + a_norm[4] * 0.45]
        assert c1.normalize(raw) == c2.normalize(raw)
        r = c2.classify_raw(raw)
        assert r["letter"] == "A"
        assert r["confidence"] > 0.9


def test_captured_templates_stored_projected():
    # set_captured_templates stores length ACTIVE_DIM, regardless of input dim.
    c = ASLClassifier()
    c.set_captured_templates({"A": ASL_TEMPLATES["A"]})  # 5-D input
    assert len(c.captured_templates["A"]) == ACTIVE_DIM


def test_load_missing_returns_false():
    c = ASLClassifier()
    assert not c.load(Path("/nonexistent/demo_calibration.json"))


def test_capture_letter_averages_samples():
    c = ASLClassifier()
    c.set_range(min=[0.4] * 5, max=[0.85] * 5, polarity=[1] * 5)
    span = 0.45
    a_norm = ASL_TEMPLATES["A"]
    a_raw = lambda: [0.40 + a_norm[i] * span for i in range(5)]
    c.capture_letter("A", [a_raw(), a_raw(), a_raw()])
    cap = c.captured_templates["A"]
    assert len(cap) == ACTIVE_DIM
    # Ring (ch3) is dropped; the other 3 active channels' norm = a_norm values.
    expect = [a_norm[i] for i in ACTIVE_CHANNELS]
    for got, exp in zip(cap, expect):
        assert abs(got - exp) < 1e-3
