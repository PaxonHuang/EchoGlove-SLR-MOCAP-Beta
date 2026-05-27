# -*- coding: utf-8 -*-
from __future__ import annotations

"""
glove_relay.src.nlp.grammar_corrector — CSL → Mandarin grammar correction.

Chinese Sign Language (CSL) has a different word-order from standard
Mandarin Chinese.  Typical CSL patterns include:

- Topic-first:  "你 名字 什么" → "你叫什么名字？"
- SOV → SVO:   "我 苹果 吃"   → "我吃苹果"
- Omission of copula: "我 学生" → "我是学生"
- Time-before-verb:  "昨天 去 学校 我" → "我昨天去学校了"

This module provides a **rule-based** corrector that converts sequences of
gesture IDs into fluent Mandarin sentences.
"""


import json
from pathlib import Path
from typing import Dict, List, Optional, Tuple

from src.utils.logger import get_logger

logger = get_logger(__name__)

# ---------------------------------------------------------------------------
# Gesture ID → word mapping (loaded from gesture_labels.json at runtime)
# ---------------------------------------------------------------------------
_LABELS_PATH = Path(__file__).resolve().parents[2] / "data" / "gesture_labels.json"


class GrammarCorrector:
    """
    Rule-based CSL grammar corrector.

    Parameters
    ----------
    labels_path : Path | None
        Path to the gesture labels JSON file.  Defaults to
        ``data/gesture_labels.json``.
    """

    def __init__(self, labels_path: Optional[Path] = None) -> None:
        self._labels_path = labels_path or _LABELS_PATH
        self._id_to_word: Dict[int, str] = {}
        self._grammar_rules: List[_GrammarRule] = []
        self._postfix_map: Dict[str, str] = {}
        self._load_labels()
        self._init_rules()

    # ------------------------------------------------------------------
    # Public API
    # ------------------------------------------------------------------
    def correct(self, gesture_ids: List[int]) -> str:
        """
        Convert a sequence of gesture IDs to a grammatically correct
        Mandarin sentence.

        Parameters
        ----------
        gesture_ids :
            List of recognised gesture IDs (may contain ``-1`` for
            unknown gestures).

        Returns
        -------
        str
            A Mandarin Chinese sentence string.
        """
        # Filter out unknown gestures
        words = [self._id_to_word.get(gid, "") for gid in gesture_ids if gid >= 0]
        words = [w for w in words if w]  # remove empty strings
        if not words:
            return ""

        # Apply grammar rules in order
        for rule in self._grammar_rules:
            words = rule.apply(words)

        sentence = "".join(words)

        # Apply postfix punctuation map
        for key, value in self._postfix_map.items():
            if sentence.endswith(key):
                sentence = sentence[: -len(key)] + value
                break

        return sentence

    # ------------------------------------------------------------------
    # Internal
    # ------------------------------------------------------------------
    def _load_labels(self) -> None:
        """Load gesture label mappings from the JSON file."""
        if not self._labels_path.exists():
            logger.warning("Gesture labels file not found: %s", self._labels_path)
            return

        with open(self._labels_path, "r", encoding="utf-8") as fh:
            labels = json.load(fh)

        for entry in labels:
            self._id_to_word[entry["id"]] = entry["name_cn"]

        logger.info("Loaded %d gesture labels", len(self._id_to_word))

    def _init_rules(self) -> None:
        """Initialise the grammar correction rules."""
        self._grammar_rules = [
            # --- Topic-comment reordering ---
            # "你 名字 什么" → "你 名字 什么" → needs insertion of 是/叫
            _InsertWordRule(["你", "名字", "什么"], "叫", index=1, remove_after=True),
            # "你 好" → "你好" (already correct, just merge)

            # --- SOV → SVO ---
            # "我 苹果 吃" → "我 吃 苹果"
            _SwapVerbObjectRule(verbs={"吃", "喝", "买", "看", "读", "写", "做", "打", "给"}),

            # --- Copula insertion ---
            # "我 学生" → "我是学生"
            _CopulaInsertionRule(
                subjects={"我", "你", "他", "她", "我们", "你们", "他们"},
                copula="是",
            ),

            # --- Time-word fronting ---
            # "昨天 去 学校 我" → "我 昨天 去 学校"
            _TimeFrontingRule(time_words={"昨天", "今天", "明天", "现在", "早上", "中午", "晚上"}),

            # --- Aspect marker ---
            # "我 去 学校" → "我去学校了" (add 了 after action)
            _AspectMarkerRule(action_verbs={"去", "来", "吃", "买", "看", "做", "打"}, marker="了"),

            # --- Question particle ---
            # "什么 名字 你" → "你叫什么名字？" (handled by postfix map)
        ]

        self._postfix_map = {
            "什么": "什么？",
            "吗": "吗？",
            "呢": "呢？",
        }


# ---------------------------------------------------------------------------
# Rule classes
# ---------------------------------------------------------------------------
class _GrammarRule:
    """Base class for grammar rules."""

    def apply(self, words: List[str]) -> List[str]:
        raise NotImplementedError


class _SwapVerbObjectRule(_GrammarRule):
    """Swap verb and object if they appear in SOV order."""

    def __init__(self, verbs: set[str]) -> None:
        self.verbs = verbs

    def apply(self, words: List[str]) -> List[str]:
        result = list(words)
        for i, w in enumerate(result):
            if w in self.verbs and i > 0:
                # Swap with previous (object)
                result[i - 1], result[i] = result[i], result[i - 1]
                break
        return result


class _CopulaInsertionRule(_GrammarRule):
    """Insert a copula (是) between subject and predicate noun."""

    def __init__(self, subjects: set[str], copula: str = "是") -> None:
        self.subjects = subjects
        self.copula = copula

    def apply(self, words: List[str]) -> List[str]:
        if len(words) == 2 and words[0] in self.subjects:
            return [words[0], self.copula, words[1]]
        return words


class _TimeFrontingRule(_GrammarRule):
    """Move time words to the beginning of the sentence."""

    def __init__(self, time_words: set[str]) -> None:
        self.time_words = time_words

    def apply(self, words: List[str]) -> List[str]:
        result = list(words)
        time_indices = [i for i, w in enumerate(result) if w in self.time_words]
        for idx in reversed(time_indices):
            word = result.pop(idx)
            result.insert(0, word)
        return result


class _AspectMarkerRule(_GrammarRule):
    """Append aspect marker (了) after the verb."""

    def __init__(self, action_verbs: set[str], marker: str = "了") -> None:
        self.action_verbs = action_verbs
        self.marker = marker

    def apply(self, words: List[str]) -> List[str]:
        result = list(words)
        for i, w in enumerate(result):
            if w in self.action_verbs and (i + 1 >= len(result) or result[i + 1] != self.marker):
                result.insert(i + 1, self.marker)
                break
        return result


class _InsertWordRule(_GrammarRule):
    """Insert a word at a specific position with optional removal."""

    def __init__(
        self,
        pattern: List[str],
        word: str,
        index: int = 0,
        remove_after: bool = False,
    ) -> None:
        self.pattern = pattern
        self.word = word
        self.index = index
        self.remove_after = remove_after

    def apply(self, words: List[str]) -> List[str]:
        if words == self.pattern:
            result = list(self.pattern)
            result.insert(self.index, self.word)
            return result
        return words
