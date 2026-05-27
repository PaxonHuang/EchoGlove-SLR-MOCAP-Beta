# -*- coding: utf-8 -*-
from __future__ import annotations

"""
glove_relay.src.models.model_registry — Central model registry with hot-switching.

The registry owns the currently-active L1 and L2 models.  It can:
  * Load models from :file:`configs/model_config.yaml`.
  * Hot-switch the active model at runtime (release old, load new).
  * Provide a singleton accessor for other components.
"""


import importlib
from pathlib import Path
from typing import Any, Dict, Optional, Type

import torch

from src.models.base_model import BaseModel
from src.utils.config import load_model_config
from src.utils.logger import get_logger

logger = get_logger(__name__)

# ---------------------------------------------------------------------------
# Mapping from model *name* → Python class path
# ---------------------------------------------------------------------------
_MODEL_CLASS_MAP: Dict[str, str] = {
    "cnn_attention_v2": "src.models.l1_cnn_attention:L1EdgeModel",
    "ms_tcn_v1": "src.models.l1_ms_tcn:MSTCNModel",
    "stgcn_v1": "src.models.stgcn_model:STGCNModel",
}


def _import_class(dotted_path: str) -> Type[BaseModel]:
    """Import a class from ``'pkg.module:ClassName'`` notation."""
    module_path, class_name = dotted_path.rsplit(":", 1)
    module = importlib.import_module(module_path)
    return getattr(module, class_name)  # type: ignore[return-value]


class ModelRegistry:
    """
    Central registry that manages L1 and L2 gesture models.

    Attributes
    ----------
    l1_model : BaseModel | None
        Currently active L1 (lightweight) model.
    l2_model : BaseModel | None
        Currently active L2 (ST-GCN fallback) model.
    active_l1_name : str
        Name key of the active L1 model from the YAML config.
    active_l2_name : str
        Name key of the active L2 model from the YAML config.
    """

    _instance: Optional[ModelRegistry] = None

    def __init__(self) -> None:
        self.l1_model: Optional[BaseModel] = None
        self.l2_model: Optional[BaseModel] = None
        self.active_l1_name: str = ""
        self.active_l2_name: str = ""
        self._l1_catalog: Dict[str, Dict[str, Any]] = {}
        self._l2_catalog: Dict[str, Dict[str, Any]] = {}
        self._device: torch.device = torch.device("cpu")

    # ------------------------------------------------------------------
    # Singleton
    # ------------------------------------------------------------------
    @classmethod
    def instance(cls) -> Optional[ModelRegistry]:
        """Return the singleton registry (``None`` until initialised)."""
        return cls._instance

    # ------------------------------------------------------------------
    # Config loading
    # ------------------------------------------------------------------
    def load_from_config(self) -> None:
        """Read YAML config, instantiate L1 and L2 models."""
        raw = load_model_config()
        ModelRegistry._instance = self

        # Determine device
        from src.utils.config import get_config
        relay_cfg = get_config()
        dev = relay_cfg.inference.device
        if dev == "auto":
            self._device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
        else:
            self._device = torch.device(dev)
        logger.info("Model device: %s", self._device)

        # Build catalogues
        self._l1_catalog = {m["name"]: m for m in raw.get("l1_models", [])}
        self._l2_catalog = {m["name"]: m for m in raw.get("l2_models", [])}

        # Load active models
        active_l1 = raw.get("active_l1_model", "")
        active_l2 = raw.get("active_l2_model", "")

        if active_l1:
            self._load_model("l1", active_l1)
        if active_l2:
            self._load_model("l2", active_l2)

        # Warm-up
        if self.l1_model is not None:
            self._warmup(self.l1_model, input_shape=(1, 21))
        if self.l2_model is not None:
            self._warmup(self.l2_model, input_shape=(1, 30, 21))

    # ------------------------------------------------------------------
    # Public API
    # ------------------------------------------------------------------
    def list_models(self) -> Dict[str, Any]:
        """Return a summary of all registered models."""
        return {
            "active_l1": self.active_l1_name,
            "active_l2": self.active_l2_name,
            "l1_models": {k: v.get("description", "") for k, v in self._l1_catalog.items()},
            "l2_models": {k: v.get("description", "") for k, v in self._l2_catalog.items()},
        }

    def switch(self, level: str, model_name: str) -> bool:
        """
        Hot-switch the active model for *level*.

        Parameters
        ----------
        level:
            ``"l1"`` or ``"l2"``.
        model_name:
            Key from the YAML model catalogue.

        Returns
        -------
        bool
            ``True`` on success.
        """
        catalog = self._l1_catalog if level == "l1" else self._l2_catalog
        if model_name not in catalog:
            logger.error("Unknown %s model: %s", level, model_name)
            return False

        old_model = self.l1_model if level == "l1" else self.l2_model
        self._load_model(level, model_name)

        # Warm-up
        new_model = self.l1_model if level == "l1" else self.l2_model
        if new_model is not None:
            shape = (1, 21) if level == "l1" else (1, 30, 21)
            self._warmup(new_model, input_shape=shape)

        # Release old
        if old_model is not None:
            del old_model
            if self._device.type == "cuda":
                torch.cuda.empty_cache()

        logger.info("Hot-switched %s → %s", level, model_name)
        return True

    def cleanup(self) -> None:
        """Release all models and free GPU memory."""
        self.l1_model = None
        self.l2_model = None
        if self._device.type == "cuda":
            torch.cuda.empty_cache()
        logger.info("Model registry cleaned up")

    # ------------------------------------------------------------------
    # Internal helpers
    # ------------------------------------------------------------------
    def _load_model(self, level: str, model_name: str) -> None:
        """Instantiate and load weights for one model."""
        catalog = self._l1_catalog if level == "l1" else self._l2_catalog
        cfg = catalog.get(model_name)
        if cfg is None:
            logger.error("Model '%s' not found in %s catalogue", model_name, level)
            return

        # Resolve class
        class_key = model_name
        if class_key not in _MODEL_CLASS_MAP:
            logger.error("No class mapping for '%s'", model_name)
            return

        cls = _import_class(_MODEL_CLASS_MAP[class_key])
        model = cls(**cfg.get("init_kwargs", {}))  # type: ignore[operator]
        model = model.to(self._device)
        model.eval()

        # Load checkpoint
        ckpt_path = Path(cfg.get("checkpoint", ""))
        if ckpt_path.exists():
            state = torch.load(ckpt_path, map_location=self._device, weights_only=True)
            model.load_state_dict(state)
            logger.info("Loaded %s checkpoint: %s", model_name, ckpt_path)
        else:
            logger.warning("Checkpoint not found: %s — using random weights", ckpt_path)

        if level == "l1":
            self.l1_model = model
            self.active_l1_name = model_name
        else:
            self.l2_model = model
            self.active_l2_name = model_name

        logger.info("Registered %s model: %s", level, model_name)

    @staticmethod
    def _warmup(model: BaseModel, input_shape: tuple[int, ...]) -> None:
        """Run one dummy forward pass to initialise lazy layers."""
        try:
            dummy = torch.randn(input_shape, device=next(model.parameters()).device)  # type: ignore[arg-type]
            with torch.no_grad():
                _ = model(dummy)
            logger.debug("Warm-up OK for %s", model.__class__.__name__)
        except Exception:
            logger.warning("Warm-up failed for %s", model.__class__.__name__, exc_info=True)
