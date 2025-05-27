from raylibpy import Color

# --- BUILDING_WALL Animation Data ---
# A simple solid square to represent a basic wall.
# This will be scaled up by the rendering system to OBJECT_SIZE.

BUILDING_WALL_MATRIX_00 = [
    [1, 1, 1, 1, 1],
    [1, 1, 1, 1, 1],
    [1, 1, 1, 1, 1],
    [1, 1, 1, 1, 1],
    [1, 1, 1, 1, 1],
]

# Color map for the wall animation.
# Index 0: Transparent (background, though not used here for a solid wall)
# Index 1: A distinct color for the wall (e.g., a dark grey or brown)
BUILDING_WALL_MAP_COLORS = [
    Color(0, 0, 0, 0),  # Transparent
    Color(80, 80, 80, 255),  # Dark Grey
]

# Animation speed for the wall (static, so speed doesn't matter much)
BUILDING_WALL_ANIMATION_SPEED = 0.1
