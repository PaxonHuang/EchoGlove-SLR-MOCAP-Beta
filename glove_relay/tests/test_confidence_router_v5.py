"""Tests for V5 three-tier Confidence Router."""
import pytest
from src.confidence_router import ConfidenceRouter


@pytest.fixture
def router():
    return ConfidenceRouter()


def test_tier1_only(router):
    result = router.route(
        tier1={"gesture_id": 5, "confidence": 0.9},
        tier2=None, tier3=None,
    )
    assert result["gesture_id"] == 5
    assert result["active_tier"] == "tier1"


def test_tier2_overrides_tier1(router):
    result = router.route(
        tier1={"gesture_id": 5, "confidence": 0.7},
        tier2={"gesture_id": 8, "confidence": 0.95},
        tier3=None,
    )
    assert result["gesture_id"] == 8
    assert result["active_tier"] == "tier2"


def test_tier3_overrides_all(router):
    result = router.route(
        tier1={"gesture_id": 5, "confidence": 0.7},
        tier2={"gesture_id": 8, "confidence": 0.8},
        tier3={"gesture_id": 12, "confidence": 0.99},
    )
    assert result["gesture_id"] == 12
    assert result["active_tier"] == "tier3"


def test_low_confidence_fallback(router):
    result = router.route(
        tier1={"gesture_id": 5, "confidence": 0.85},
        tier2={"gesture_id": 8, "confidence": 0.3},
        tier3=None,
    )
    assert result["active_tier"] == "tier1"


def test_hot_swap_blend(router):
    old = {"gesture_id": 5, "confidence": 0.9}
    new = {"gesture_id": 8, "confidence": 0.95}
    for frame in range(5):
        result = router.route(
            tier1=old if frame < 2 else new,
            tier2=new if frame >= 2 else None,
            tier3=None,
        )
    assert result["active_tier"] == "tier2"
