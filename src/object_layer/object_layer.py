from enum import Enum
from dataclasses import dataclass, field
from typing import List


class Direction(Enum):
    UP = 0
    UP_RIGHT = 1
    RIGHT = 2
    DOWN_RIGHT = 3
    DOWN = 4
    DOWN_LEFT = 5
    LEFT = 6
    UP_LEFT = 7
    NONE = 8


class ObjectLayerMode(Enum):
    IDLE = 0
    WALKING = 1
    TELEPORTING = 2


# Stats structure equivalent
@dataclass
class Stats:
    effect: int = 0
    resistance: int = 0
    agility: int = 0
    range: int = 0
    intelligence: int = 0
    utility: int = 0


# RenderFrames holds named animation frames as 3D int matrices.
@dataclass
class RenderFrames:
    up_idle: List[List[List[int]]] = field(default_factory=list)
    down_idle: List[List[List[int]]] = field(default_factory=list)
    right_idle: List[List[List[int]]] = field(default_factory=list)
    left_idle: List[List[List[int]]] = field(default_factory=list)
    up_right_idle: List[List[List[int]]] = field(default_factory=list)
    down_right_idle: List[List[List[int]]] = field(default_factory=list)
    up_left_idle: List[List[List[int]]] = field(default_factory=list)
    down_left_idle: List[List[List[int]]] = field(default_factory=list)
    default_idle: List[List[List[int]]] = field(default_factory=list)
    up_walking: List[List[List[int]]] = field(default_factory=list)
    down_walking: List[List[List[int]]] = field(default_factory=list)
    right_walking: List[List[List[int]]] = field(default_factory=list)
    left_walking: List[List[List[int]]] = field(default_factory=list)
    up_right_walking: List[List[List[int]]] = field(default_factory=list)
    down_right_walking: List[List[List[int]]] = field(default_factory=list)
    up_left_walking: List[List[List[int]]] = field(default_factory=list)
    down_left_walking: List[List[List[int]]] = field(default_factory=list)
    none_idle: List[List[List[int]]] = field(default_factory=list)


# Render holds the frames, palette/colors and timing metadata.
@dataclass
class Render:
    frames: RenderFrames = field(default_factory=RenderFrames)
    colors: List[List[int]] = field(default_factory=list)
    frame_duration: int = 0
    is_stateless: bool = False


# Item describes the item this layer represents.
@dataclass
class Item:
    id: str = ""
    type: str = ""
    description: str = ""
    activable: bool = False


# ObjectLayerData encapsulates stats, render, and item.
@dataclass
class ObjectLayerData:
    stats: Stats = field(default_factory=Stats)
    render: Render = field(default_factory=Render)
    item: Item = field(default_factory=Item)


# ObjectLayer encapsulates object layer data and a content hash.
@dataclass
class ObjectLayer:
    data: ObjectLayerData = field(default_factory=ObjectLayerData)
    sha256: str = ""


# ObjectLayerState describes an object layer's state for an entity.
@dataclass
class ObjectLayerState:
    item_id: str = ""
    active: bool = False
