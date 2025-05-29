import logging
from enum import Enum, auto

# This file centralizes object layer data, enums, and related constants.
logging.basicConfig(
    level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s"
)

# Import actual object layer matrices and color maps
from object_layer.building.wall import (
    BUILDING_WALL_ANIMATION_SPEED,
    BUILDING_WALL_MAP_COLORS,
    BUILDING_WALL_MATRIX_00,
)
from object_layer.gfx.click_pointer import (
    GFX_CLICK_POINTER_ANIMATION_SPEED,
    GFX_CLICK_POINTER_MAP_COLORS,
    GFX_CLICK_POINTER_MATRIX_00,
    GFX_CLICK_POINTER_MATRIX_01,
    GFX_CLICK_POINTER_MATRIX_02,
)
from object_layer.gfx.point_path import (
    GFX_POINT_PATH_ANIMATION_SPEED,
    GFX_POINT_PATH_MAP_COLORS,
    GFX_POINT_PATH_MATRIX_00,
)
from object_layer.skin.people import (
    SKIN_PEOPLE_ANIMATION_SPEED,
    SKIN_PEOPLE_MAP_COLORS,
    SKIN_PEOPLE_MATRIX_02_0,
    SKIN_PEOPLE_MATRIX_02_1,
    SKIN_PEOPLE_MATRIX_06_0,
    SKIN_PEOPLE_MATRIX_06_1,
    SKIN_PEOPLE_MATRIX_08_0,
    SKIN_PEOPLE_MATRIX_08_1,
    SKIN_PEOPLE_MATRIX_12_0,
    SKIN_PEOPLE_MATRIX_12_1,
    SKIN_PEOPLE_MATRIX_16_0,
    SKIN_PEOPLE_MATRIX_16_1,
    SKIN_PEOPLE_MATRIX_18_0,
    SKIN_PEOPLE_MATRIX_18_1,
)


class Direction(Enum):
    """Defines possible directions for animated objects."""

    UP = auto()
    UP_RIGHT = auto()
    RIGHT = auto()
    DOWN_RIGHT = auto()
    DOWN = auto()
    DOWN_LEFT = auto()
    LEFT = auto()
    UP_LEFT = auto()
    NONE = auto()  # For objects without a specific direction (e.g., static)


class ObjectLayerMode(Enum):
    """Defines animation modes (e.g., idle, walking)."""

    IDLE = auto()
    WALKING = auto()


OBJECT_LAYER_DATA = {
    "PEOPLE": {
        "FRAMES": {
            "UP_IDLE": [SKIN_PEOPLE_MATRIX_02_0, SKIN_PEOPLE_MATRIX_02_1],
            "DOWN_IDLE": [SKIN_PEOPLE_MATRIX_08_0, SKIN_PEOPLE_MATRIX_08_1],
            "RIGHT_IDLE": [SKIN_PEOPLE_MATRIX_06_0, SKIN_PEOPLE_MATRIX_06_1],
            "LEFT_IDLE": [SKIN_PEOPLE_MATRIX_06_0, SKIN_PEOPLE_MATRIX_06_1],
            "UP_RIGHT_IDLE": [SKIN_PEOPLE_MATRIX_06_0, SKIN_PEOPLE_MATRIX_06_1],
            "DOWN_RIGHT_IDLE": [SKIN_PEOPLE_MATRIX_06_0, SKIN_PEOPLE_MATRIX_06_1],
            "UP_LEFT_IDLE": [SKIN_PEOPLE_MATRIX_06_0, SKIN_PEOPLE_MATRIX_06_1],
            "DOWN_LEFT_IDLE": [SKIN_PEOPLE_MATRIX_06_0, SKIN_PEOPLE_MATRIX_06_1],
            "DEFAULT_IDLE": [
                SKIN_PEOPLE_MATRIX_08_0,
                SKIN_PEOPLE_MATRIX_08_1,
            ],  # Fallback
            "UP_WALKING": [SKIN_PEOPLE_MATRIX_12_0, SKIN_PEOPLE_MATRIX_12_1],
            "DOWN_WALKING": [SKIN_PEOPLE_MATRIX_18_0, SKIN_PEOPLE_MATRIX_18_1],
            "RIGHT_WALKING": [SKIN_PEOPLE_MATRIX_16_0, SKIN_PEOPLE_MATRIX_16_1],
            "LEFT_WALKING": [SKIN_PEOPLE_MATRIX_16_0, SKIN_PEOPLE_MATRIX_16_1],
            "UP_RIGHT_WALKING": [SKIN_PEOPLE_MATRIX_16_0, SKIN_PEOPLE_MATRIX_16_1],
            "DOWN_RIGHT_WALKING": [SKIN_PEOPLE_MATRIX_16_0, SKIN_PEOPLE_MATRIX_16_1],
            "UP_LEFT_WALKING": [SKIN_PEOPLE_MATRIX_16_0, SKIN_PEOPLE_MATRIX_16_1],
            "DOWN_LEFT_WALKING": [SKIN_PEOPLE_MATRIX_16_0, SKIN_PEOPLE_MATRIX_16_1],
        },
        "COLORS": SKIN_PEOPLE_MAP_COLORS,
        "FRAME_DURATION": SKIN_PEOPLE_ANIMATION_SPEED,
        "IS_STATELESS": False,  # People animations have state (direction, mode)
    },
    "CLICK_POINTER": {
        "FRAMES": {
            "NONE_IDLE": [  # Stateless animations use NONE_IDLE or DEFAULT_IDLE
                GFX_CLICK_POINTER_MATRIX_00,
                GFX_CLICK_POINTER_MATRIX_01,
                GFX_CLICK_POINTER_MATRIX_02,
            ],
            "DEFAULT_IDLE": [  # Fallback
                GFX_CLICK_POINTER_MATRIX_00,
                GFX_CLICK_POINTER_MATRIX_01,
                GFX_CLICK_POINTER_MATRIX_02,
            ],
        },
        "COLORS": GFX_CLICK_POINTER_MAP_COLORS,
        "FRAME_DURATION": GFX_CLICK_POINTER_ANIMATION_SPEED,
        "IS_STATELESS": True,  # Click pointer animation is stateless
    },
    "POINT_PATH": {
        "FRAMES": {
            "NONE_IDLE": [GFX_POINT_PATH_MATRIX_00],
            "DEFAULT_IDLE": [GFX_POINT_PATH_MATRIX_00],  # Fallback
        },
        "COLORS": GFX_POINT_PATH_MAP_COLORS,
        "FRAME_DURATION": GFX_POINT_PATH_ANIMATION_SPEED,
        "IS_STATELESS": True,  # Path points are stateless
    },
    "WALL": {
        "FRAMES": {
            "NONE_IDLE": [BUILDING_WALL_MATRIX_00],
            "DEFAULT_IDLE": [BUILDING_WALL_MATRIX_00],  # Fallback
        },
        "COLORS": BUILDING_WALL_MAP_COLORS,
        "FRAME_DURATION": BUILDING_WALL_ANIMATION_SPEED,
        "IS_STATELESS": True,  # Walls are static and stateless
    },
}
