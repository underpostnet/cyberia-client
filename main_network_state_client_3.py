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
        self.grid_w = GRID_SIZE_W
        self.grid_h = GRID_SIZE_H
        self.aoi_radius = AOI_RADIUS
        self.obstacles = {}

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
                        for player_id, obj_state in visible_players_data.items():
                            pos = obj_state.get("Pos", {})
                            dims = obj_state.get("Dims", {})
                            self.game_state.other_players[player_id] = {
                                "pos": pr.Vector2(pos.get("X", 0.0), pos.get("Y", 0.0)),
                                "dims": pr.Vector2(
                                    dims.get("Width", DEFAULT_OBJECT_WIDTH),
                                    dims.get("Height", DEFAULT_OBJECT_HEIGHT),
                                ),
                                # Store the direction and mode
                                "direction": Direction(
                                    obj_state.get("direction", Direction.NONE.value)
                                ),
                                "mode": ObjectLayerMode(
                                    obj_state.get("mode", ObjectLayerMode.IDLE.value)
                                ),
                            }

                        # FIX: The client now correctly updates its obstacle list with only
                        # the obstacles within its AOI.
                        visible_obstacles_data = payload.get("visibleGridObjects", {})
                        self.game_state.obstacles = {}
                        for obj_id, obj_state in visible_obstacles_data.items():
                            pos = obj_state.get("Pos", {})
                            dims = obj_state.get("Dims", {})
                            self.game_state.obstacles[obj_id] = {
                                "pos": pr.Vector2(pos.get("X", 0.0), pos.get("Y", 0.0)),
                                "dims": pr.Vector2(
                                    dims.get("Width", DEFAULT_OBJECT_WIDTH),
                                    dims.get("Height", DEFAULT_OBJECT_HEIGHT),
                                ),
                            }

                elif message_type == "path_not_found":
                    message_text = data.get("payload", "Path not found.")
                    self.game_state.last_error_message = message_text
                    self.game_state.error_display_time = time.time()
                    print(f"Server Feedback: {message_text}")
                else:
                    pass  # Ignore unknown messages to reduce log spam

            except json.JSONDecodeError as e:
                # FIX: Handle JSON errors gracefully by logging the full message.
                # This helps debug the server, which is likely sending malformed JSON.
                print(f"Failed to decode JSON: {e}")
                print(f"Received message: {message}")
                self.game_state.last_error_message = f"JSON Decode Error: {e}"
                self.game_state.error_display_time = time.time()
            except Exception as e:
                print(f"An error occurred: {e}")
                self.game_state.last_error_message = f"An Occurred Error: {e}"
                self.game_state.error_display_time = time.time()

    def on_error(self, ws, error):
        print(f"WebSocket Error: {error}")

    def on_close(self, ws, close_status_code, close_msg):
        print(f"WebSocket closed with status {close_status_code}: {close_msg}")

    def on_open(self, ws):
        print("WebSocket connection opened.")

    def send_path_request(self, target_x, target_y):
        message = {
            "type": "path_request",
            "payload": {"targetX": target_x, "targetY": target_y},
        }
        json_message = json.dumps(message)
        with self.mutex:
            if self.ws:
                self.ws.send(json_message)
                self.game_state.upload_size_bytes += len(json_message)

    def draw_grid(self):
        if not DEV_GUI:
            return
        grid_w = self.game_state.grid_w
        grid_h = self.game_state.grid_h

        # Draw grid lines
        for x in range(grid_w):
            pr.draw_line_v(
                pr.Vector2(x * CELL_SIZE, 0),
                pr.Vector2(x * CELL_SIZE, grid_h * CELL_SIZE),
                pr.GRAY,
            )
        for y in range(grid_h):
            pr.draw_line_v(
                pr.Vector2(0, y * CELL_SIZE),
                pr.Vector2(grid_w * CELL_SIZE, y * CELL_SIZE),
                pr.GRAY,
            )

    def draw_game_objects(self):
        with self.mutex:
            # Draw obstacles
            for obj_state in self.game_state.obstacles.values():
                pos = obj_state["pos"]
                dims = obj_state["dims"]
                pr.draw_rectangle(
                    int(pos.x * CELL_SIZE),
                    int(pos.y * CELL_SIZE),
                    int(dims.x * CELL_SIZE),
                    int(dims.y * CELL_SIZE),
                    COLOR_OBSTACLE,
                )

            # Interpolate player position
            time_since_update = time.time() - self.game_state.last_update_time
            if time_since_update < INTERPOLATION_TIME_MS / 1000.0:
                # Calculate interpolation factor
                factor = time_since_update / (INTERPOLATION_TIME_MS / 1000.0)
                self.game_state.player_pos_interpolated.x = (
                    self.game_state.player_pos_prev.x
                    + (
                        self.game_state.player_pos_server.x
                        - self.game_state.player_pos_prev.x
                    )
                    * factor
                )
                self.game_state.player_pos_interpolated.y = (
                    self.game_state.player_pos_prev.y
                    + (
                        self.game_state.player_pos_server.y
                        - self.game_state.player_pos_prev.y
                    )
                    * factor
                )
            else:
                self.game_state.player_pos_interpolated = (
                    self.game_state.player_pos_server
                )

            # Draw the main player with a slightly different color for walking/idle state
            pos = self.game_state.player_pos_interpolated
            dims = self.game_state.player_dims

            player_color = COLOR_PLAYER
            if self.game_state.player_mode == ObjectLayerMode.WALKING:
                player_color = pr.Color(0, 255, 0, 255)  # Green when walking

            pr.draw_rectangle_v(
                pr.Vector2(
                    pos.x * CELL_SIZE,
                    pos.y * CELL_SIZE,
                ),
                pr.Vector2(dims.x * CELL_SIZE, dims.y * CELL_SIZE),
                player_color,
            )

            # Draw other players
            for player_id, obj_state in self.game_state.other_players.items():
                pos = obj_state["pos"]
                dims = obj_state["dims"]
                # Change color for other players
                other_player_color = COLOR_OTHER_PLAYER
                if obj_state["mode"] == ObjectLayerMode.WALKING:
                    other_player_color = pr.Color(
                        255, 100, 255, 255
                    )  # Purple when walking

                pr.draw_rectangle_v(
                    pr.Vector2(
                        pos.x * CELL_SIZE,
                        pos.y * CELL_SIZE,
                    ),
                    pr.Vector2(dims.x * CELL_SIZE, dims.y * CELL_SIZE),
                    other_player_color,
                )

            # Draw path
            if len(self.game_state.path) > 0:
                current_pos = self.game_state.player_pos_interpolated
                for point in self.game_state.path:
                    # FIX: Use draw_line_ex for thicker lines
                    pr.draw_line_ex(
                        pr.Vector2(
                            current_pos.x * CELL_SIZE + (dims.x * CELL_SIZE / 2),
                            current_pos.y * CELL_SIZE + (dims.y * CELL_SIZE / 2),
                        ),
                        pr.Vector2(
                            point.x * CELL_SIZE + (dims.x * CELL_SIZE / 2),
                            point.y * CELL_SIZE + (dims.y * CELL_SIZE / 2),
                        ),
                        2.0,  # line thickness
                        COLOR_PATH,
                    )
                    current_pos = point

            # Draw target
            if self.game_state.target_pos.x != -1:
                pr.draw_circle(
                    int(
                        self.game_state.target_pos.x * CELL_SIZE
                        + (dims.x * CELL_SIZE / 2)
                    ),
                    int(
                        self.game_state.target_pos.y * CELL_SIZE
                        + (dims.y * CELL_SIZE / 2)
                    ),
                    int(CELL_SIZE / 4),
                    COLOR_TARGET,
                )

    def draw_aoi(self):
        if not DEV_GUI:
            return

        # FIX: Draw a filled, semi-transparent rectangle instead of an outline.
        pr.draw_rectangle(
            int(self.game_state.aoi_rect.x * CELL_SIZE),
            int(self.game_state.aoi_rect.y * CELL_SIZE),
            int(self.game_state.aoi_rect.width * CELL_SIZE),
            int(self.game_state.aoi_rect.height * CELL_SIZE),
            COLOR_AOI,
        )

    def draw_debug_info(self, fps, download_kbps, upload_kbps):
        pr.draw_text(f"FPS: {fps}", 10, 10, 20, COLOR_DEBUG_TEXT)
        pr.draw_text(
            f"Download: {download_kbps:.2f} KB/s", 10, 40, 20, COLOR_DEBUG_TEXT
        )
        pr.draw_text(f"Upload: {upload_kbps:.2f} KB/s", 10, 70, 20, COLOR_DEBUG_TEXT)
        # FIX: Display the enum name instead of the integer value
        pr.draw_text(
            f"Mode: {self.game_state.player_mode.name}", 10, 100, 20, COLOR_DEBUG_TEXT
        )
        pr.draw_text(
            f"Direction: {self.game_state.player_direction.name}",
            10,
            130,
            20,
            COLOR_DEBUG_TEXT,
        )

        # Draw error message if present
        if (
            self.game_state.last_error_message
            and (time.time() - self.game_state.error_display_time) < 5.0
        ):
            pr.draw_text(
                self.game_state.last_error_message, 10, 160, 20, COLOR_ERROR_TEXT
            )

    def run_game_loop(self):
        # --- Pyray Initialization ---
        # Initialize the window with the client-side constants.
        pr.init_window(SCREEN_WIDTH, SCREEN_HEIGHT, "Network State Client")
        pr.set_target_fps(FPS)

        # --- WebSocket connection ---
        try:
            self.ws = websocket.WebSocketApp(
                WS_URL,
                on_open=self.on_open,
                on_message=self.on_message,
                on_error=self.on_error,
                on_close=self.on_close,
            )
        except Exception as e:
            print(f"Failed to create WebSocketApp: {e}")
            self.is_running = False
            pr.close_window()
            return

        self.ws_thread = threading.Thread(target=self.ws.run_forever)
        self.ws_thread.daemon = True
        self.ws_thread.start()

        # --- Performance Metric Tracking ---
        last_download_check = time.time()
        last_upload_check = time.time()

        # --- Main game loop ---
        while not pr.window_should_close() and self.is_running:
            # Recenter camera on player
            with self.mutex:
                self.game_state.camera.target = pr.Vector2(
                    self.game_state.player_pos_interpolated.x * CELL_SIZE
                    + self.game_state.player_dims.x * CELL_SIZE / 2,
                    self.game_state.player_pos_interpolated.y * CELL_SIZE
                    + self.game_state.player_dims.y * CELL_SIZE / 2,
                )

            # Update performance metrics
            current_time = time.time()
            if current_time - last_download_check >= 1.0:
                with self.mutex:
                    self.download_kbps = self.game_state.download_size_bytes / 1024
                    self.game_state.download_size_bytes = 0
                last_download_check = current_time
            if current_time - last_upload_check >= 1.0:
                with self.mutex:
                    self.upload_kbps = self.game_state.upload_size_bytes / 1024
                    self.game_state.upload_size_bytes = 0
                last_upload_check = current_time

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

            pr.begin_drawing()
            pr.clear_background(COLOR_BACKGROUND)

            pr.begin_mode_2d(self.game_state.camera)
            self.draw_grid()
            self.draw_game_objects()
            self.draw_aoi()
            pr.end_mode_2d()

            # Draw UI on top of the 2D world
            self.draw_debug_info(pr.get_fps(), self.download_kbps, self.upload_kbps)
            pr.end_drawing()

        # --- Clean up resources ---
        print("Closing WebSocket...")
        self.ws.close()

        self.ws_thread.join(timeout=3)
        if self.ws_thread.is_alive():
            print("WebSocket thread did not close gracefully. Forcing exit.")

        self.is_running = False
        pr.close_window()


def main():
    client = NetworkClient()
    client.run_game_loop()


if __name__ == "__main__":
    main()
