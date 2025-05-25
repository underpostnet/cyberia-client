# --- Instance Constants ---
SERVER_HOST = "127.0.0.1"
SERVER_PORT = 5000
WEBSOCKET_PATH = "/ws"  # The endpoint for WebSocket connections on the Go server

# Raylib window dimensions (camera viewport)
SCREEN_WIDTH = 800
SCREEN_HEIGHT = 800

# World dimensions (the grid/canvas)
WORLD_WIDTH = 1600
WORLD_HEIGHT = 1600

# Size of instance objects (players, obstacles)
OBJECT_SIZE = 50

# A* maze simplification: each cell in the 32x32 maze represents a block of this size in the world
MAZE_CELL_WORLD_SIZE = WORLD_WIDTH // 32  # 1600 / 32 = 50

# Camera smoothing factor (0.0 to 1.0, higher means faster smoothing)
CAMERA_SMOOTHNESS = 0.05
