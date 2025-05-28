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
# Number of recent instantaneous directions to consider for smoothing
# an object's visual animation direction. A higher value results in
# smoother, less jittery direction changes but can introduce a slight
# visual lag. This helps mitigate rapid, small positional updates
# from the server or client-side interpolation.
DIRECTION_HISTORY_LENGTH = 15
# Smoothness factor for camera movement, between 0.0 (no movement) and 1.0 (instant movement).
CAMERA_SMOOTHNESS = 0.1


# --- Game Object Type Definitions ---
# These constants define the object types and their server-priority status.
# Server-priority objects (like players and walls) have their state
# authoritative from the server. Client-priority objects (like click pointers
# and path points) are managed and decay on the client side.

# Mapping of object types to their server-priority status
SERVER_PRIORITY_OBJECT_TYPES = {
    "PLAYER": True,
    "WALL": True,
    "POINT_PATH": False,
    "CLICK_POINTER": False,
    "UNKNOWN": False,
}

# --- Object Type to Display IDs Mapping ---
# This mapping defines which display IDs should be assigned to each object type
# when an object is created or updated from server data, if display_ids are not provided.
# A displayID can refer to a set of up to 16 animations (8 directions * 2 modes)
# or 1 animation if it's stateless.
OBJECT_TYPE_DEFAULT_DISPLAY_IDS = {
    "PLAYER": ["PEOPLE"],
    "WALL": ["WALL"],
    "POINT_PATH": ["POINT_PATH"],
    "CLICK_POINTER": ["CLICK_POINTER"],
    "UNKNOWN": [],
}
