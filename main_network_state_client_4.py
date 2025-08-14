import pyray as pr
import websocket
import threading
import json
import time
import math
import sys
from enum import Enum, auto

# --- Game Constants ---
# These are client-side defaults and will be updated by the server
# The window will now be initialized with these values and resized
# later if the server provides different grid dimensions.
SCREEN_WIDTH = 1280
SCREEN_HEIGHT = 800
GRID_SIZE_W = 100
GRID_SIZE_H = 100
DEFAULT_OBJECT_WIDTH = 1.0
DEFAULT_OBJECT_HEIGHT = 1.0

# Calculated values
CELL_SIZE = 10.0  # Assumed size for drawing grid cells
FPS = 60
WS_URL = "ws://localhost:8080/ws"
# The client will interpolate between server updates. This value determines how long it takes.
# Changed to match the new server tick rate for smoother movement.
INTERPOLATION_TIME_MS = 50

# Dev GUI Flag
# Set to True to enable debug rendering (grid, AOI)
DEV_GUI = True
AOI_RADIUS = 20.0  # Client-side assumption for AOI radius, in grid units

# Colors
COLOR_BACKGROUND = pr.Color(30, 30, 30, 255)
COLOR_OBSTACLE = pr.Color(100, 100, 100, 255)
COLOR_PLAYER = pr.Color(0, 200, 255, 255)
COLOR_OTHER_PLAYER = pr.Color(255, 100, 0, 255)
COLOR_PATH = pr.fade(pr.GREEN, 0.5)
COLOR_TARGET = pr.Color(255, 255, 0, 255)
# FIX: Changed AOI color to a filled, semi-transparent purple.
COLOR_AOI = pr.fade(pr.PURPLE, 0.2)
COLOR_DEBUG_TEXT = pr.Color(200, 200, 200, 255)
COLOR_ERROR_TEXT = pr.Color(255, 50, 50, 255)  # Red color for error messages
COLOR_PORTAL = pr.Color(150, 0, 255, 150)  # New color for portals


# --- New Enums for Animation State ---
# FIX: Explicitly define enum values to match the server's Go `iota` behavior (0-indexed).
class Direction(Enum):
    """Defines possible directions for animated objects."""

    UP = 0
    UP_RIGHT = 1
    RIGHT = 2
    DOWN_RIGHT = 3
    DOWN = 4
    DOWN_LEFT = 5
    LEFT = 6
    UP_LEFT = 7
    NONE = 8  # For objects without a specific direction (e.g., static)


class ObjectLayerMode(Enum):
    """Defines animation modes (e.g., idle, walking)."""

    IDLE = 0
    WALKING = 1


# --- Game State Management ---
class GameState:
    def __init__(self):
        # Mutex for thread-safe access to shared data
        self.mutex = threading.Lock()

        # Server-provided data
        self.player_id = None
        self.player_map_id = 0  # NEW: The player's current map ID
        self.grid_w = GRID_SIZE_W
        self.grid_h = GRID_SIZE_H
        self.aoi_radius = AOI_RADIUS
        self.obstacles = {}
        self.portals = {}  # NEW: Dictionary to store portals

        # Dynamic game state from server updates
        # Player position for rendering (interpolated)
        self.player_pos_interpolated = pr.Vector2(0, 0)
        # Last known position from the server
        self.player_pos_server = pr.Vector2(0, 0)
        # Previous position from the server
        self.player_pos_prev = pr.Vector2(0, 0)
        self.player_dims = pr.Vector2(DEFAULT_OBJECT_WIDTH, DEFAULT_OBJECT_HEIGHT)
        self.player_direction = Direction.NONE
        self.player_mode = ObjectLayerMode.IDLE
        self.other_players = {}
        self.path = []
        self.target_pos = pr.Vector2(-1, -1)
        self.aoi_rect = pr.Rectangle(0, 0, 0, 0)
        self.last_update_time = time.time()
        self.last_error_message = ""
        self.error_display_time = 0.0

        # Performance metrics
        self.download_size_bytes = 0
        self.upload_size_bytes = 0


class NetworkClient:
    def __init__(self):
        self.game_state = GameState()
        self.ws = None
        self.ws_thread = None
        self.is_running = True
        self.mutex = self.game_state.mutex
        self.download_kbps = 0.0
        self.upload_kbps = 0.0
        self.player_id = None

        # Initialize camera with the defined SCREEN_WIDTH and SCREEN_HEIGHT
        self.game_state.camera = pr.Camera2D(
            pr.Vector2(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2),  # offset
            pr.Vector2(0, 0),  # target
            0.0,  # rotation
            2.0,  # zoom - Changed from 1.0 to 2.0
        )

    def on_message(self, ws, message):
        with self.mutex:
            try:
                data = json.loads(message)
                message_type = data.get("type")
                self.game_state.download_size_bytes += len(message)

                if message_type == "init_data":
                    payload = data.get("payload", {})
                    if payload:
                        new_grid_w = payload.get("gridW", self.game_state.grid_w)
                        new_grid_h = payload.get("gridH", self.game_state.grid_h)
                        self.game_state.aoi_radius = payload.get(
                            "aoiRadius", self.game_state.aoi_radius
                        )
                        # Check if the grid size has changed and resize the window
                        if (
                            new_grid_w != self.game_state.grid_w
                            or new_grid_h != self.game_state.grid_h
                        ):
                            self.game_state.grid_w = new_grid_w
                            self.game_state.grid_h = new_grid_h
                            pr.set_window_size(
                                self.game_state.grid_w * int(CELL_SIZE),
                                self.game_state.grid_h * int(CELL_SIZE),
                            )

                        # Update camera offset for the new window size
                        self.game_state.camera.offset = pr.Vector2(
                            pr.get_screen_width() / 2, pr.get_screen_height() / 2
                        )

                        # FIX: No obstacles are sent in init_data anymore.
                        # The client will receive its first obstacle list in the initial AOI update.
                        self.game_state.obstacles = {}

                elif message_type == "aoi_update":
                    payload = data.get("payload", {})
                    if payload:
                        self.player_id = payload.get("playerID")

                        # Update player's own state for interpolation
                        player_data = payload.get("player", {})
                        if player_data:
                            self.game_state.player_id = player_data.get("id")
                            self.game_state.player_map_id = player_data.get("MapID", 0)
                            pos = player_data.get("Pos", {})

                            # Store the previous server position before updating
                            self.game_state.player_pos_prev = (
                                self.game_state.player_pos_server
                            )

                            self.game_state.player_pos_server.x = pos.get("X", 0.0)
                            self.game_state.player_pos_server.y = pos.get("Y", 0.0)

                            self.game_state.last_update_time = time.time()

                            # NEW: Player dimensions are now dynamic
                            dims = player_data.get("Dims", {})
                            self.game_state.player_dims.x = dims.get(
                                "Width", DEFAULT_OBJECT_WIDTH
                            )
                            self.game_state.player_dims.y = dims.get(
                                "Height", DEFAULT_OBJECT_HEIGHT
                            )

                            self.game_state.path = [
                                pr.Vector2(p.get("X"), p.get("Y"))
                                for p in player_data.get("path", [])
                            ]
                            target_pos = player_data.get("targetPos", {})
                            self.game_state.target_pos.x = target_pos.get("X", -1)
                            self.game_state.target_pos.y = target_pos.get("Y", -1)

                            # Update the AOI rectangle
                            aoi_rect_data = payload.get("player", {}).get("AOI", {})
                            self.game_state.aoi_rect.x = aoi_rect_data.get("MinX", 0.0)
                            self.game_state.aoi_rect.y = aoi_rect_data.get("MinY", 0.0)
                            self.game_state.aoi_rect.width = aoi_rect_data.get(
                                "MaxX", 0.0
                            ) - aoi_rect_data.get("MinX", 0.0)
                            self.game_state.aoi_rect.height = aoi_rect_data.get(
                                "MaxY", 0.0
                            ) - aoi_rect_data.get("MinY", 0.0)

                            # NEW: Update player animation state with error handling
                            try:
                                self.game_state.player_direction = Direction(
                                    player_data.get("direction", Direction.NONE.value)
                                )
                                self.game_state.player_mode = ObjectLayerMode(
                                    player_data.get("mode", ObjectLayerMode.IDLE.value)
                                )
                            except ValueError as ve:
                                self.game_state.last_error_message = (
                                    f"Enum Conversion Error: {ve}"
                                )
                                self.game_state.error_display_time = time.time()
                                print(self.game_state.last_error_message)

                        # Update visible players
                        visible_players_data = payload.get("visiblePlayers", {})
                        # Clear old players to avoid rendering players that have moved out of AOI
                        self.game_state.other_players = {}
                        for player_id, p_data in visible_players_data.items():
                            pos = p_data.get("Pos", {})
                            dims = p_data.get("Dims", {})
                            # Check if the player is not the client's own player
                            if player_id != self.game_state.player_id:
                                self.game_state.other_players[player_id] = {
                                    "pos": pr.Vector2(pos.get("X"), pos.get("Y")),
                                    "dims": pr.Vector2(
                                        dims.get("Width"), dims.get("Height")
                                    ),
                                }

                        # Update visible grid objects (obstacles and now portals)
                        visible_objects_data = payload.get("visibleGridObjects", {})
                        # Clear old objects
                        self.game_state.obstacles = {}
                        self.game_state.portals = {}
                        for obj_id, obj_data in visible_objects_data.items():
                            obj_type = obj_data.get("Type")
                            pos = obj_data.get("Pos", {})
                            dims = obj_data.get("Dims", {})
                            obj = {
                                "pos": pr.Vector2(pos.get("X"), pos.get("Y")),
                                "dims": pr.Vector2(
                                    dims.get("Width"), dims.get("Height")
                                ),
                            }
                            if obj_type == "obstacle":
                                self.game_state.obstacles[obj_id] = obj
                            elif obj_type == "portal":
                                self.game_state.portals[obj_id] = obj

            except json.JSONDecodeError as e:
                self.game_state.last_error_message = f"JSON Decode Error: {e}"
                self.game_state.error_display_time = time.time()
            except Exception as e:
                self.game_state.last_error_message = f"Error processing message: {e}"
                self.game_state.error_display_time = time.time()

    def on_error(self, ws, error):
        print(f"### error ###: {error}")

    def on_close(self, ws, close_status_code, close_msg):
        print(f"### closed ###: {close_status_code} - {close_msg}")
        self.is_running = False

    def on_open(self, ws):
        print("Opened WebSocket connection")

    def run_websocket_thread(self):
        self.ws = websocket.WebSocketApp(
            WS_URL,
            on_open=self.on_open,
            on_message=self.on_message,
            on_error=self.on_error,
            on_close=self.on_close,
        )
        # Set ping interval and timeout to keep the connection alive
        self.ws.run_forever(ping_interval=20, ping_timeout=10)
        print("WebSocket thread finished.")

    def start(self):
        pr.init_window(SCREEN_WIDTH, SCREEN_HEIGHT, "Networked Game Client")
        pr.set_target_fps(FPS)
        # Don't start the thread if the window fails to open
        if not pr.is_window_ready():
            return

        self.ws_thread = threading.Thread(target=self.run_websocket_thread)
        self.ws_thread.start()

        self.run_game_loop()

    def draw_grid(self):
        """Draws the game grid."""
        for x in range(0, self.game_state.grid_w + 1):
            pr.draw_line(
                int(x * CELL_SIZE),
                0,
                int(x * CELL_SIZE),
                int(self.game_state.grid_h * CELL_SIZE),
                pr.fade(pr.LIGHTGRAY, 0.2),
            )
        for y in range(0, self.game_state.grid_h + 1):
            pr.draw_line(
                0,
                int(y * CELL_SIZE),
                int(self.game_state.grid_w * CELL_SIZE),
                int(y * CELL_SIZE),
                pr.fade(pr.LIGHTGRAY, 0.2),
            )

    def draw_portals(self):
        """Draws all visible portals."""
        for portal_id, portal_data in self.game_state.portals.items():
            pos = portal_data["pos"]
            dims = portal_data["dims"]
            pr.draw_rectangle_rec(
                pr.Rectangle(
                    pos.x * CELL_SIZE,
                    pos.y * CELL_SIZE,
                    dims.x * CELL_SIZE,
                    dims.y * CELL_SIZE,
                ),
                COLOR_PORTAL,
            )

    def draw_game_objects(self):
        """Draws all game objects (obstacles, players)."""
        # Draw all obstacles
        for obstacle_id, obstacle_data in self.game_state.obstacles.items():
            pos = obstacle_data["pos"]
            dims = obstacle_data["dims"]
            pr.draw_rectangle_rec(
                pr.Rectangle(
                    pos.x * CELL_SIZE,
                    pos.y * CELL_SIZE,
                    dims.x * CELL_SIZE,
                    dims.y * CELL_SIZE,
                ),
                COLOR_OBSTACLE,
            )

        # Draw other players
        for player_id, player_data in self.game_state.other_players.items():
            pos = player_data["pos"]
            dims = player_data["dims"]
            pr.draw_rectangle_rec(
                pr.Rectangle(
                    pos.x * CELL_SIZE,
                    pos.y * CELL_SIZE,
                    dims.x * CELL_SIZE,
                    dims.y * CELL_SIZE,
                ),
                COLOR_OTHER_PLAYER,
            )

        # Draw the main player last so they are always on top
        pos = self.game_state.player_pos_interpolated
        dims = self.game_state.player_dims
        pr.draw_rectangle_rec(
            pr.Rectangle(
                pos.x * CELL_SIZE,
                pos.y * CELL_SIZE,
                dims.x * CELL_SIZE,
                dims.y * CELL_SIZE,
            ),
            COLOR_PLAYER,
        )

        # Draw the target position
        if self.game_state.target_pos.x != -1:
            pr.draw_rectangle_rec(
                pr.Rectangle(
                    self.game_state.target_pos.x * CELL_SIZE,
                    self.game_state.target_pos.y * CELL_SIZE,
                    CELL_SIZE,
                    CELL_SIZE,
                ),
                COLOR_TARGET,
            )

        # Draw the path
        if DEV_GUI and len(self.game_state.path) > 1:
            for i in range(len(self.game_state.path) - 1):
                pr.draw_line_ex(
                    pr.Vector2(
                        self.game_state.path[i].x * CELL_SIZE + CELL_SIZE / 2,
                        self.game_state.path[i].y * CELL_SIZE + CELL_SIZE / 2,
                    ),
                    pr.Vector2(
                        self.game_state.path[i + 1].x * CELL_SIZE + CELL_SIZE / 2,
                        self.game_state.path[i + 1].y * CELL_SIZE + CELL_SIZE / 2,
                    ),
                    2.0,
                    COLOR_PATH,
                )

    def draw_aoi(self):
        """Draws the player's Area of Interest."""
        if DEV_GUI:
            aoi_rect = self.game_state.aoi_rect
            pr.draw_rectangle_rec(
                pr.Rectangle(
                    aoi_rect.x * CELL_SIZE,
                    aoi_rect.y * CELL_SIZE,
                    aoi_rect.width * CELL_SIZE,
                    aoi_rect.height * CELL_SIZE,
                ),
                COLOR_AOI,
            )

    def draw_debug_info(self, fps, download_kbps, upload_kbps):
        """Draws debug information on the screen."""
        if DEV_GUI:
            pr.draw_text(f"FPS: {fps}", 10, 10, 20, COLOR_DEBUG_TEXT)
            pr.draw_text(
                f"Download: {download_kbps:.2f} kbps", 10, 30, 20, COLOR_DEBUG_TEXT
            )
            pr.draw_text(
                f"Upload: {upload_kbps:.2f} kbps", 10, 50, 20, COLOR_DEBUG_TEXT
            )
            pr.draw_text(
                f"Grid: {self.game_state.grid_w}x{self.game_state.grid_h}",
                10,
                70,
                20,
                COLOR_DEBUG_TEXT,
            )
            pr.draw_text(
                f"Map ID: {self.game_state.player_map_id}", 10, 90, 20, COLOR_DEBUG_TEXT
            )

        if time.time() - self.game_state.error_display_time < 5.0:
            pr.draw_text(
                self.game_state.last_error_message,
                10,
                pr.get_screen_height() - 30,
                20,
                COLOR_ERROR_TEXT,
            )

    def send_path_request(self, grid_x, grid_y):
        """Sends a path request message to the server."""
        if self.ws and self.ws.sock and self.ws.sock.connected:
            message = {
                "type": "path_request",
                "payload": {"targetPos": {"X": grid_x, "Y": grid_y}},
            }
            try:
                # Calculate size before sending for upload rate calculation
                json_message = json.dumps(message)
                self.game_state.upload_size_bytes += len(json_message)
                self.ws.send(json_message)
            except Exception as e:
                self.game_state.last_error_message = f"Failed to send message: {e}"
                self.game_state.error_display_time = time.time()

    def run_game_loop(self):
        """The main game loop for rendering and input handling."""
        last_kbps_update = time.time()
        last_frame_time = time.time()

        while not pr.window_should_close() and self.is_running:
            current_time = time.time()
            delta_time = current_time - last_frame_time
            last_frame_time = current_time

            # Update performance metrics every second
            if current_time - last_kbps_update >= 1.0:
                self.download_kbps = self.game_state.download_size_bytes * 8 / 1024
                self.upload_kbps = self.game_state.upload_size_bytes * 8 / 1024
                self.game_state.download_size_bytes = 0
                self.game_state.upload_size_bytes = 0
                last_kbps_update = current_time

            with self.mutex:
                # Interpolate player position
                interpolation_factor = (
                    (current_time - self.game_state.last_update_time) * 1000
                ) / INTERPOLATION_TIME_MS

                # Clamp interpolation factor to prevent overshooting
                interpolation_factor = min(interpolation_factor, 1.0)

                self.game_state.player_pos_interpolated.x = pr.lerp(
                    self.game_state.player_pos_prev.x,
                    self.game_state.player_pos_server.x,
                    interpolation_factor,
                )
                self.game_state.player_pos_interpolated.y = pr.lerp(
                    self.game_state.player_pos_prev.y,
                    self.game_state.player_pos_server.y,
                    interpolation_factor,
                )

                # FIX: Update the camera's target to follow the player's interpolated position.
                # The target is the center of the player rectangle, scaled by CELL_SIZE.
                self.game_state.camera.target = pr.Vector2(
                    self.game_state.player_pos_interpolated.x * CELL_SIZE
                    + self.game_state.player_dims.x * CELL_SIZE / 2,
                    self.game_state.player_pos_interpolated.y * CELL_SIZE
                    + self.game_state.player_dims.y * CELL_SIZE / 2,
                )

            # Handle user input
            if pr.is_mouse_button_pressed(pr.MOUSE_BUTTON_LEFT):
                mouse_pos = pr.get_mouse_position()
                world_pos = pr.get_screen_to_world_2d(mouse_pos, self.game_state.camera)
                # Map world coordinates to grid coordinates
                grid_x = math.floor(world_pos.x / CELL_SIZE)
                grid_y = math.floor(world_pos.y / CELL_SIZE)

                if (
                    0 <= grid_x < self.game_state.grid_w
                    and 0 <= grid_y < self.game_state.grid_h
                ):
                    self.send_path_request(grid_x, grid_y)

            # FIX: Wrap the entire drawing block in a mutex lock to prevent race conditions.
            pr.begin_drawing()
            pr.clear_background(COLOR_BACKGROUND)

            pr.begin_mode_2d(self.game_state.camera)
            # All drawing functions must use the mutex to access game state safely
            with self.mutex:
                self.draw_grid()
                self.draw_portals()
                self.draw_game_objects()
                self.draw_aoi()
            pr.end_mode_2d()

            # Draw UI on top of the 2D world
            with self.mutex:
                self.draw_debug_info(pr.get_fps(), self.download_kbps, self.upload_kbps)
            pr.end_drawing()

        # --- Clean up resources ---
        print("Closing WebSocket...")
        self.ws.close()

        self.ws_thread.join(timeout=3)
        if self.ws_thread.is_alive():
            print("WebSocket thread did not close gracefully. Forcing exit.")

        pr.close_window()


if __name__ == "__main__":
    client = NetworkClient()
    client.start()
