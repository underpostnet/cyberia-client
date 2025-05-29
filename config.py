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

# --- Client-side Animation Smoothing Settings ---
DIRECTION_HISTORY_LENGTH = 15
CAMERA_SMOOTHNESS = 0.1

# --- Object type default object layer IDs mapping ---
# This mapping defines which object layer ID should be assigned to each object type
# when an object is created or updated from server data, if object_layer_ids are not provided.
# An objectLayerId can refer to a set of up to 16 animations (8 directions * 2 modes)
# or 1 animation if it's stateless.
OBJECT_TYPE_DEFAULT_OBJECT_LAYER_IDS = {
    "PLAYER": ["PEOPLE"],
    "WALL": ["WALL"],
    "POINT_PATH": ["POINT_PATH"],
    "CLICK_POINTER": ["CLICK_POINTER"],
    "UNKNOWN": [],
}
