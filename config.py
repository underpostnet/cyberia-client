# --- Screen and World Dimensions ---
SCREEN_WIDTH = 1280
SCREEN_HEIGHT = 720
WORLD_WIDTH = 1600
WORLD_HEIGHT = 1600
OBJECT_SIZE = 50
MAZE_CELL_WORLD_SIZE = 50

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
DIRECTION_HISTORY_LENGTH = 15  # Adjusted for less sensitivity
