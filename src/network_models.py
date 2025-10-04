from dataclasses import dataclass, field
from typing import List, Dict, Any, Optional
import pyray as pr

from src.object_layer.object_layer import Direction, ObjectLayerMode, ObjectLayerState


@dataclass
class ColorRGBA:
    r: int = 255
    g: int = 255
    b: int = 255
    a: int = 255


@dataclass
class Point:
    X: float = 0.0
    Y: float = 0.0


@dataclass
class PointI:
    X: int = 0
    Y: int = 0


@dataclass
class Dimensions:
    Width: float = 0.0
    Height: float = 0.0


@dataclass
class PlayerObject:
    id: str
    MapID: int
    Pos: Point
    Dims: Dimensions
    direction: Direction
    mode: ObjectLayerMode
    objectLayers: List[ObjectLayerState] = field(default_factory=list)
    life: float = 100.0
    maxLife: float = 100.0
    sumStatsLimit: int = 9999
    path: List[PointI] = field(default_factory=list)
    targetPos: PointI = field(default_factory=PointI)
    respawnIn: Optional[float] = None


@dataclass
class VisiblePlayer:
    id: str
    Pos: Point
    Dims: Dimensions
    direction: Direction
    mode: ObjectLayerMode
    objectLayers: List[ObjectLayerState] = field(default_factory=list)
    life: float = 100.0
    maxLife: float = 100.0
    respawnIn: Optional[float] = None


@dataclass
class VisibleBot:
    id: str
    Pos: Point
    Dims: Dimensions
    direction: Direction
    mode: ObjectLayerMode
    behavior: str
    objectLayers: List[ObjectLayerState] = field(default_factory=list)
    life: float = 100.0
    maxLife: float = 100.0
    respawnIn: Optional[float] = None


@dataclass
class VisibleFloor:
    id: str
    Pos: Point
    Dims: Dimensions
    Type: str
    objectLayers: List[ObjectLayerState] = field(default_factory=list)


@dataclass
class VisibleObject:
    id: str
    Pos: Point
    Dims: Dimensions
    Type: str
    PortalLabel: Optional[str] = None


@dataclass
class AOIUpdatePayload:
    player: PlayerObject
    visiblePlayers: Dict[str, VisiblePlayer] = field(default_factory=dict)
    visibleGridObjects: Dict[str, Any] = field(default_factory=dict)


@dataclass
class SkillItemIdsPayload:
    requestedItemId: str = ""
    associatedItemIds: List[str] = field(default_factory=list)


@dataclass
class InitPayload:
    gridW: int = 100
    gridH: int = 100
    defaultObjectWidth: float = 1.0
    defaultObjectHeight: float = 1.0
    cellSize: float = 12.0
    fps: int = 60
    interpolationMs: int = 200
    aoiRadius: float = 15.0
    colors: Dict[str, ColorRGBA] = field(default_factory=dict)
    cameraSmoothing: float = 0.15
    cameraZoom: float = 1.0
    defaultWidthScreenFactor: float = 0.5
    defaultHeightScreenFactor: float = 0.5
    devUi: bool = False
    sumStatsLimit: int = 9999
    # This field is in the Go struct but not used in the provided client logic.
    # Adding it for completeness.
    objectLayers: List[ObjectLayerState] = field(default_factory=list)
