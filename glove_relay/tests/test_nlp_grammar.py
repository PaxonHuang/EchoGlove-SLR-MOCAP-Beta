# -*- coding: utf-8 -*-
"""Tests for GrammarCorrector — CSL → Mandarin rule-based correction."""

import json
import pytest
from pathlib import Path

from src.nlp.grammar_corrector import GrammarCorrector


# ---------------------------------------------------------------------------
# Fixtures
# ---------------------------------------------------------------------------
@pytest.fixture
def labels_path(tmp_path: Path) -> Path:
    """Create a minimal gesture labels file for testing."""
    labels = [
        {"id": 10, "name_cn": "你", "name_en": "you"},
        {"id": 11, "name_cn": "我", "name_en": "I"},
        {"id": 12, "name_cn": "他", "name_en": "he"},
        {"id": 14, "name_cn": "好", "name_en": "good"},
        {"id": 20, "name_cn": "名字", "name_en": "name"},
        {"id": 21, "name_cn": "什么", "name_en": "what"},
        {"id": 22, "name_cn": "学生", "name_en": "student"},
        {"id": 24, "name_cn": "学校", "name_en": "school"},
        {"id": 26, "name_cn": "吃", "name_en": "eat"},
        {"id": 27, "name_cn": "喝", "name_en": "drink"},
        {"id": 29, "name_cn": "苹果", "name_en": "apple"},
        {"id": 31, "name_cn": "去", "name_en": "go"},
    ]
    p = tmp_path / "gesture_labels.json"
    p.write_text(json.dumps(labels, ensure_ascii=False), encoding="utf-8")
    return p


@pytest.fixture
def corrector(labels_path: Path) -> GrammarCorrector:
    return GrammarCorrector(labels_path=labels_path)


# ---------------------------------------------------------------------------
# Basic ID → word mapping
# ---------------------------------------------------------------------------
class TestIdMapping:
    def test_single_word(self, corrector: GrammarCorrector):
        assert corrector.correct([10]) == "你"

    def test_multiple_words(self, corrector: GrammarCorrector):
        result = corrector.correct([10, 14])  # 你 好
        assert "你" in result
        assert "好" in result

    def test_unknown_id_filtered(self, corrector: GrammarCorrector):
        result = corrector.correct([-1, 10, 999])
        assert result == "你"

    def test_empty_input(self, corrector: GrammarCorrector):
        assert corrector.correct([]) == ""

    def test_all_unknown(self, corrector: GrammarCorrector):
        assert corrector.correct([-1, -1]) == ""


# ---------------------------------------------------------------------------
# SOV → SVO verb-object swap
# ---------------------------------------------------------------------------
class TestSOVtoSVO:
    def test_me_apple_eat(self, corrector: GrammarCorrector):
        # CSL: 我 苹果 吃 → Mandarin: 我 吃 苹果
        result = corrector.correct([11, 29, 26])  # 我 苹果 吃
        idx_eat = result.index("吃")
        idx_apple = result.index("苹果")
        assert idx_eat < idx_apple, f"Expected 吃 before 苹果, got: {result}"

    def test_already_correct_order(self, corrector: GrammarCorrector):
        # If verb is already first (no object before it), no swap
        result = corrector.correct([26])  # just 吃
        assert "吃" in result


# ---------------------------------------------------------------------------
# Copula insertion (是 between subject and predicate noun)
# ---------------------------------------------------------------------------
class TestCopulaInsertion:
    def test_i_student(self, corrector: GrammarCorrector):
        # 我 学生 → 我 是 学生
        result = corrector.correct([11, 22])  # 我 学生
        assert "是" in result
        assert result.index("是") == 1

    def test_you_student(self, corrector: GrammarCorrector):
        # 你 学生 → 你 是 学生
        result = corrector.correct([10, 22])  # 你 学生
        assert "是" in result

    def test_no_copula_for_verb(self, corrector: GrammarCorrector):
        # 我 吃 — should NOT insert 是 (吃 is a verb, not a predicate noun)
        result = corrector.correct([11, 26])  # 我 吃
        # SwapVerbObject may swap, but copula should not fire for 3-word output
        assert "是" not in result or len(result) > 3


# ---------------------------------------------------------------------------
# Question postfix
# ---------------------------------------------------------------------------
class TestQuestionPostfix:
    def test_what_gets_question_mark(self, corrector: GrammarCorrector):
        result = corrector.correct([21])  # 什么
        assert result.endswith("？")

    def test_no_question_mark_for_statement(self, corrector: GrammarCorrector):
        result = corrector.correct([11, 22])  # 我 学生
        assert not result.endswith("？")


# ---------------------------------------------------------------------------
# Edge cases
# ---------------------------------------------------------------------------
class TestEdgeCases:
    def test_missing_labels_file(self, tmp_path: Path):
        """GrammarCorrector should handle missing labels gracefully."""
        gc = GrammarCorrector(labels_path=tmp_path / "nonexistent.json")
        result = gc.correct([10, 11])
        assert result == ""  # no labels → no words

    def test_single_verb_no_swap(self, corrector: GrammarCorrector):
        result = corrector.correct([26])  # just 吃
        assert "吃" in result

    def test_duplicate_ids(self, corrector: GrammarCorrector):
        result = corrector.correct([10, 10])  # 你 你
        assert "你" in result
