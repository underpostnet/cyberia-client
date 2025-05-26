# --- Screen and World Dimensions ---
SCREEN_WIDTH = 1280
SCREEN_HEIGHT = 720
WORLD_WIDTH = 1600
WORLD_HEIGHT = 1600
OBJECT_SIZE = 50  # Size of a single game object (e.g., player, obstacle) in pixels

# --- Maze and Grid Settings ---
# This defines the size of a cell in the simplified maze used for obstacle visualization.
# It should be a divisor of WORLD_WIDTH and WORLD_HEIGHT.
MAZE_CELL_WORLD_SIZE = 50  # For a 32x32 maze in a 1600x1600 world, 1600/50 = 32

# --- Server Connection Settings ---
SERVER_HOST = "localhost"
SERVER_PORT = 5000
WEBSOCKET_PATH = "/ws"

# --- Camera Settings ---
CAMERA_SMOOTHNESS = 0.1  # Value between 0.0 and 1.0 for camera interpolation smoothness

# --- Animation Settings (General) ---
# These can be overridden by specific animation data, but provide sensible defaults
DEFAULT_FRAME_DURATION = (
    0.25  # Default time in seconds each animation frame is displayed
)
DEFAULT_STOP_DELAY_DURATION = (
    0.25  # Default delay before idle animation after stopping movement
)
