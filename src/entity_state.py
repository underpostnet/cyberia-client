from dataclasses import dataclass, field
from typing import List, Dict, Any
import pyray as pr
from src.object_layer.object_layer import Direction, ObjectLayerMode
import time


@dataclass
class EntityState:
    """Base class for any dynamic entity in the game."""

    id: str
    pos_server: pr.Vector2
    pos_prev: pr.Vector2
    interp_pos: pr.Vector2
    dims: pr.Vector2
    direction: Direction
    mode: ObjectLayerMode
    object_layers: List[Dict[str, Any]] = field(default_factory=list)
    life: float = 100.0
    max_life: float = 100.0
    respawn_in: float = 0.0
    last_update: float = field(default_factory=time.time)


@dataclass
class PlayerState(EntityState):
    """Represents the state of the main player."""

    map_id: int = 0
    path: List[pr.Vector2] = field(default_factory=list)
    target_pos: pr.Vector2 = field(default=pr.Vector2(-1, -1))


@dataclass
class BotState(EntityState):
    """Represents the state of a bot."""

    behavior: str = "passive"
