# -*- coding: utf-8 -*-
"""
glove_relay.src.models.base_model — Abstract base class for all gesture models.

Every L1 and L2 model **must** inherit from :class:`BaseModel` and implement
its four abstract methods.  The class is designed to be combined with
``torch.nn.Module`` via multiple inheritance.
"""

from __future__ import annotations

from abc import ABC, abstractmethod
from typing import Any

import torch


class BaseModel(ABC):
    """
    Abstract interface for gesture recognition models.

    Concrete classes should typically inherit from *both* ``BaseModel`` and
    ``torch.nn.Module`` so that standard PyTorch training utilities (e.g.
    ``torch.save``, ``.to(device)``) work out of the box.

    Example
    -------
    >>> class MyModel(BaseModel, torch.nn.Module):
    ...     def __init__(self):
    ...         super().__init__()
    ...         self.fc = torch.nn.Linear(11, 46)
    ...     def forward(self, x):
    ...         return self.fc(x)
    ...     def predict(self, x):
    ...         logits = self.forward(x)
    ...         prob = torch.softmax(logits, dim=-1)
    ...         conf, idx = prob.max(dim=-1)
    ...         return idx.item(), conf.item()
    ...     def get_config(self):
    ...         return {"input_dim": 11, "num_classes": 46}
    ...     def get_model_info(self):
    ...         return {"name": "MyModel", "params": sum(p.numel() for p in self.parameters())}
    """

    @abstractmethod
    def forward(self, x: torch.Tensor) -> torch.Tensor:
        """
        Run a forward pass and return raw logits.

        Parameters
        ----------
        x:
            Input tensor.  Shape depends on the model:
            - L1: ``(batch, 11)``  (single-hand V6 feature vector)
            - L2: ``(batch, 22)``  (concatenated [left(11) ‖ right(11)])

        Returns
        -------
        torch.Tensor
            Logits of shape ``(batch, num_classes)``.
        """
        ...

    @abstractmethod
    def predict(self, x: torch.Tensor) -> tuple[int, float]:
        """
        Run inference and return the top-1 prediction.

        Parameters
        ----------
        x:
            Same as :meth:`forward`.

        Returns
        -------
        tuple[int, float]
            ``(gesture_id, confidence)`` where *gesture_id* is the class
            index and *confidence* is the softmax probability of that class.
        """
        ...

    @abstractmethod
    def get_config(self) -> dict[str, Any]:
        """
        Return a dictionary describing the model architecture hyper-parameters.

        This is useful for reproducibility and for the model registry.
        """
        ...

    @abstractmethod
    def get_model_info(self) -> dict[str, Any]:
        """
        Return a dictionary with model metadata (name, parameter count, etc.).
        """
        ...
