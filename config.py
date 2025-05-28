# --- Screen and World Dimensions ---
SCREEN_WIDTH = 1280
SCREEN_HEIGHT = 720
WORLD_WIDTH = 1600
WORLD_HEIGHT = 1600
OBJECT_SIZE = 50  # Standard size for all game objects in pixels (e.g., 50x50 pixels)
MAZE_CELL_WORLD_SIZE = 50  # Size of a maze cell for A* pathfinding (50x50 pixels)

# --- Server Connection Settings ---
SERVER_HOST = "localhost"
SERVER_PORT = 5000
WEBSOCKET_PATH = "/ws"

# --- Animation Smoothing Settings ---
DIRECTION_HISTORY_LENGTH = 15
# Smoothness factor for camera movement, between 0.0 (no movement) and 1.0 (instant movement).
CAMERA_SMOOTHNESS = 0.1
