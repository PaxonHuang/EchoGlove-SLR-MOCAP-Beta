# -*- coding: utf-8 -*-
"""Tests for the rule-based demo classifier."""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from src.inference.rule_classifier import RuleClassifier, GESTURE_IDS, GESTURE_TEXT


def _f(thumb, index, middle, ring, pinky):
    return [thumb, index, middle, ring, pinky]


def test_open_hand():
    c = RuleClassifier()
    r = c.classify(_f(0.0, 0.0, 0.0, 0.0, 0.0))
    assert r["label"] == "open_hand"
    assert r["gesture_id"] == 0
    assert r["text"] == "无手势"
    assert r["confidence"] > 0.9


def test_fist():
    c = RuleClassifier()
    r = c.classify(_f(1.0, 1.0, 1.0, 1.0, 1.0))
    assert r["label"] == "fist"
    assert r["gesture_id"] == 13  # GESTURE_LABELS[13] = '好'
    assert r["text"] == "好"
    assert r["confidence"] > 0.9


def test_thumbs_up():
    c = RuleClassifier()
    r = c.classify(_f(0.0, 1.0, 1.0, 1.0, 1.0))
    assert r["label"] == "thumbs_up"
    assert r["text"] == "赞"
    assert r["confidence"] > 0.9


def test_point_one():
    c = RuleClassifier()
    r = c.classify(_f(1.0, 0.0, 1.0, 1.0, 1.0))
    assert r["label"] == "point"
    assert r["text"] == "一"


def test_peace_two():
    c = RuleClassifier()
    r = c.classify(_f(1.0, 0.0, 0.0, 1.0, 1.0))
    assert r["label"] == "peace"
    assert r["text"] == "二"


def test_three():
    c = RuleClassifier()
    r = c.classify(_f(1.0, 0.0, 0.0, 0.0, 1.0))
    assert r["label"] == "three"
    assert r["text"] == "三"


def test_four():
    c = RuleClassifier()
    r = c.classify(_f(1.0, 0.0, 0.0, 0.0, 0.0))
    assert r["label"] == "four"
    assert r["text"] == "四"


def test_pinky_up_five():
    c = RuleClassifier()
    r = c.classify(_f(1.0, 1.0, 1.0, 1.0, 0.0))
    assert r["label"] == "pinky_up"
    assert r["text"] == "五"


def test_dead_band_returns_unknown():
    # 0.5 is in the dead band (neither >=0.6 nor <=0.4) → no rule matches.
    c = RuleClassifier()
    r = c.classify(_f(0.5, 0.5, 0.5, 0.5, 0.5))
    assert r["gesture_id"] == 0
    assert r["confidence"] < 0.5


def test_noisy_still_classifies():
    # Slight noise around a fist should still classify as fist.
    c = RuleClassifier()
    r = c.classify(_f(0.9, 0.85, 0.92, 0.88, 0.91))
    assert r["label"] == "fist"


def test_custom_thresholds():
    # Looser thresholds still separate cleanly.
    c = RuleClassifier(bent=0.55, straight=0.45)
    r = c.classify(_f(0.0, 0.5, 0.5, 0.5, 0.5))
    # thumb=0 straight (≤0.45), others=0.5 not bent (<0.55) and not straight
    # → thumbs_up fails (others must be bent) → unknown.
    assert r["gesture_id"] == 0


def test_all_signs_have_ids_and_text():
    # Sanity: every label the classifier can return has an id + text.
    for label, gid in GESTURE_IDS.items():
        assert isinstance(gid, int)
        assert label in GESTURE_TEXT
        assert isinstance(GESTURE_TEXT[label], str) and len(GESTURE_TEXT[label]) > 0


def test_short_input_padded():
    c = RuleClassifier()
    r = c.classify([0.0])  # only thumb provided
    # padded to [0,0,0,0,0] → open_hand
    assert r["label"] == "open_hand"


def test_clamps_out_of_range():
    c = RuleClassifier()
    r = c.classify(_f(-1.0, 2.0, 0.0, 0.0, 0.0))
    # clamped to [0,1,0,0,0]: index bent, rest straight → no rule (needs all-bent
    # for fist, or specific combos) → unknown.
    assert r["gesture_id"] == 0
