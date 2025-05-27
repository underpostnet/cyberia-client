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

# --- Game Object Type Definitions ---
# These constants define the object types and their server-priority status.
# Server-priority objects (like players and walls) have their state
# authoritative from the server. Client-priority objects (like click pointers
# and path points) are managed and decay on the client side.
OBJECT_TYPE_PLAYER = "PLAYER"
OBJECT_TYPE_WALL = "WALL"
OBJECT_TYPE_POINT_PATH = "POINT_PATH"
OBJECT_TYPE_CLICK_POINTER = "CLICK_POINTER"

# Mapping of object types to their server-priority status
SERVER_PRIORITY_OBJECT_TYPES = {
    OBJECT_TYPE_PLAYER: True,
    OBJECT_TYPE_WALL: True,
    OBJECT_TYPE_POINT_PATH: False,
    OBJECT_TYPE_CLICK_POINTER: False,
    "UNKNOWN": False,  # Default for any undefined object type
}

# --- Animation Asset IDs ---
# These are the unique identifiers for different animation sets,
# which are used by the RenderingSystem to fetch and display animations.
ANIMATION_ASSET_PEOPLE = "SKIN_PEOPLE"
ANIMATION_ASSET_WALL = "BUILDING_WALL"
ANIMATION_ASSET_CLICK_POINTER = "CLICK_POINTER"
ANIMATION_ASSET_POINT_PATH = "POINT_PATH"
