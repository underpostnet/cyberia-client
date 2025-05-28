# data/animations/gfx/point_path.py

from raylibpy import Color

# --- GFX_POINT_PATH Animation Data ---
# A simple 3x3 pixel square to represent a path point.
# This will be scaled up by the rendering system.

GFX_POINT_PATH_MATRIX_00 = [
    [0, 0, 0],
    [0, 1, 0],
    [0, 0, 0],
]

# Color map for the point path animation.
# Index 0: Transparent (background)
# Index 1: A distinct color for the path point (e.g., a light blue or green)
GFX_POINT_PATH_MAP_COLORS = [
    Color(0, 0, 0, 0),  # Transparent
    Color(0, 200, 255, 180),  # Light Blue with some transparency
]

# Animation speed for the point path (if it were to animate, but it's static for now)
# Setting a very high value effectively makes it a single-frame animation.
GFX_POINT_PATH_ANIMATION_SPEED = 0.1
