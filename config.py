# Screen and World Dimensions
SCREEN_WIDTH = 1280
SCREEN_HEIGHT = 720
WORLD_WIDTH = 1600
WORLD_HEIGHT = 1600
NETWORK_OBJECT_SIZE = 50
MAZE_CELL_WORLD_SIZE = 50

# Server Connection Settings
SERVER_HOST = "localhost"
SERVER_PORT = 5000
WEBSOCKET_PATH = "/ws"

# Client-side Object Rendering Smoothing
DIRECTION_HISTORY_LENGTH = 15
CAMERA_SMOOTHNESS = 0.1

# Network object type to object layer IDs mapping
NETWORK_OBJECT_TYPE_DEFAULT_OBJECT_LAYER_IDS = {
    "PLAYER": ["PEOPLE"],
    "WALL": ["WALL"],
    "POINT_PATH": ["POINT_PATH"],
    "CLICK_POINTER": ["CLICK_POINTER"],
    "UNKNOWN": [],
}
