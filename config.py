# Screen and World Dimensions
SCREEN_WIDTH = 1700
SCREEN_HEIGHT = 1000
WORLD_WIDTH = 3000
WORLD_HEIGHT = 3000
NETWORK_OBJECT_SIZE = 50
MAZE_CELL_WORLD_SIZE = 50

# Map Viewport Settings
MAP_VIEWPORT_WIDTH = 300
MAP_VIEWPORT_HEIGHT = 300
# MAP_OBJECT_SIZE_PX removed: The size of map objects/cells will now be dynamically calculated
# Removed reduced zoom levels for performance and visual clarity
MAP_ZOOM_LEVELS = [0.1, 0.2, 0.5, 1.0, 2.0]  # Zoom factors: 1.0x, 2.0x


# Server Connection Settings
SERVER_HOST = "localhost"
SERVER_PORT = 5000
WEBSOCKET_PATH = "/ws"

# Client-side Object Rendering Smoothing
DIRECTION_HISTORY_LENGTH = 15
CAMERA_SMOOTHNESS = 0.1

# UI Settings
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

# UI Route Definitions (for RouterCoreComponent)
# Each route defines a UI panel/modal and its associated navigation icon.
# 'name': Display name for the navigation button.
# 'path': Unique identifier for the route.
# 'icon_path': Path to the icon image for the navigation button.
# 'view_instance': Placeholder for the actual view class instance (e.g., BagCyberiaView).
# 'render_callback': Reference to the function that renders the view's content.
UI_ROUTES = [
    {
        "name": "Character",
        "path": "/character",
        "icon_path": "ui/assets/icons/character.png",
        "view_instance": None,  # Will be set in NetworkStateClient
        "render_callback": None,  # Will be set in NetworkStateClient
    },
    {
        "name": "Bag",
        "path": "/bag",
        "icon_path": "ui/assets/icons/bag.png",
        "view_instance": None,  # Will be set in NetworkStateClient
        "render_callback": None,  # Will be set in NetworkStateClient
    },
    {
        "name": "Chat",
        "path": "/chat",
        "icon_path": "ui/assets/icons/chat.png",
        "view_instance": None,  # Will be set in NetworkStateClient
        "render_callback": None,  # Will be set in NetworkStateClient
    },
    {
        "name": "Quest",
        "path": "/quest",
        "icon_path": "ui/assets/icons/quest.png",
        "view_instance": None,  # Will be set in NetworkStateClient
        "render_callback": None,  # Will be set in NetworkStateClient
    },
    {
        "name": "Map",
        "path": "/map",
        "icon_path": "ui/assets/icons/map.png",
        "view_instance": None,  # Will be set in NetworkStateClient
        "render_callback": None,  # Will be set in NetworkStateClient
    },
]
