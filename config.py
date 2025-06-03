# Screen and World Dimensions
SCREEN_WIDTH = 1280
SCREEN_HEIGHT = 720
WORLD_WIDTH = 3000
WORLD_HEIGHT = 3000
NETWORK_OBJECT_SIZE = 50
MAZE_CELL_WORLD_SIZE = 50

# Server Connection Settings
SERVER_HOST = "localhost"
SERVER_PORT = 5000
WEBSOCKET_PATH = "/ws"

# Client-side Object Rendering Smoothing
DIRECTION_HISTORY_LENGTH = 15
CAMERA_SMOOTHNESS = 0.1

# UI Settings
UI_MODAL_WIDTH = 280
UI_MODAL_HEIGHT = 80
UI_MODAL_PADDING_TOP = 5
UI_MODAL_PADDING_RIGHT = 5
UI_MODAL_BACKGROUND_COLOR = (
    0,
    0,
    0,
    150,
)  # Semi-transparent black for modal background
UI_TEXT_COLOR_PRIMARY = (255, 204, 0, 255)  # RGBA for yellow/orange
UI_TEXT_COLOR_SHADING = (0, 0, 0, 255)  # RGBA for black (shading)
UI_FONT_SIZE = 18  # Reduced font size for all UI text

# Network object type to object layer IDs mapping
NETWORK_OBJECT_TYPE_DEFAULT_OBJECT_LAYER_IDS = {
    "PLAYER": ["ACTION_AREA", "ANON"],
    "WALL": ["WALL"],
    "POINT_PATH": ["POINT_PATH"],
    "CLICK_POINTER": ["CLICK_POINTER"],
    "BOT-QUEST-PROVIDER": ["AYLEEN"],
    "UNKNOWN": [],
}
