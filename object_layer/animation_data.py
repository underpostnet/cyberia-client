import logging
from enum import Enum, auto

# This file centralizes animation data, enums, and related constants.
# In a full-fledged application, this data might be loaded from external
# asset files or a content management system.

logging.basicConfig(
    level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s"
)

# Import actual animation matrices and color maps
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
    """
    Defines the 8 cardinal and intercardinal directions, plus a NONE state.
    Used for character animation and movement.
    """

    UP = auto()
    UP_RIGHT = auto()
    RIGHT = auto()
    DOWN_RIGHT = auto()
    DOWN = auto()
    DOWN_LEFT = auto()
    LEFT = auto()
    UP_LEFT = auto()
    NONE = auto()


class AnimationMode(Enum):
    """
    Defines the animation modes for objects, e.g., idle or walking.
    """

    IDLE = auto()
    WALKING = auto()


# ANIMATION_DATA structure:
# {
#   "OBJECT_LAYER_ID": {
#     "FRAMES": {
#       "DIRECTION_MODE": [matrix_frame_0, matrix_frame_1, ...],
#       ...
#     },
#     "COLORS": [(R, G, B, A), ...], # List of raw RGBA tuples for color mapping
#     "FRAME_DURATION": float,       # Time in seconds each frame is displayed
#     "IS_STATELESS": bool           # True if animation ignores direction/mode
#   },
#   ...
# }
#
# Each matrix_frame is a list of lists representing pixel data,
# where each integer corresponds to an index in the "COLORS" list.

ANIMATION_DATA = {
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
            "DEFAULT_IDLE": [SKIN_PEOPLE_MATRIX_08_0, SKIN_PEOPLE_MATRIX_08_1],
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
        "IS_STATELESS": False,
    },
    "CLICK_POINTER": {
        "FRAMES": {
            "NONE_IDLE": [
                GFX_CLICK_POINTER_MATRIX_00,
                GFX_CLICK_POINTER_MATRIX_01,
                GFX_CLICK_POINTER_MATRIX_02,
            ],
            "DEFAULT_IDLE": [
                GFX_CLICK_POINTER_MATRIX_00,
                GFX_CLICK_POINTER_MATRIX_01,
                GFX_CLICK_POINTER_MATRIX_02,
            ],
        },
        "COLORS": GFX_CLICK_POINTER_MAP_COLORS,
        "FRAME_DURATION": GFX_CLICK_POINTER_ANIMATION_SPEED,
        "IS_STATELESS": True,
    },
    "POINT_PATH": {
        "FRAMES": {
            "NONE_IDLE": [GFX_POINT_PATH_MATRIX_00],
            "DEFAULT_IDLE": [GFX_POINT_PATH_MATRIX_00],
        },
        "COLORS": GFX_POINT_PATH_MAP_COLORS,
        "FRAME_DURATION": GFX_POINT_PATH_ANIMATION_SPEED,
        "IS_STATELESS": True,
    },
    "WALL": {
        "FRAMES": {
            "NONE_IDLE": [BUILDING_WALL_MATRIX_00],
            "DEFAULT_IDLE": [BUILDING_WALL_MATRIX_00],
        },
        "COLORS": BUILDING_WALL_MAP_COLORS,
        "FRAME_DURATION": BUILDING_WALL_ANIMATION_SPEED,
        "IS_STATELESS": True,
    },
}
