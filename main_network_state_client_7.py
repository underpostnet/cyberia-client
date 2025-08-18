import pyray as pr
import websocket
import threading
import json
import time
import math
import sys
from enum import Enum

# --- Game Constants ---
SCREEN_WIDTH = 1280
SCREEN_HEIGHT = 800
GRID_SIZE_W = 100
GRID_SIZE_H = 100
DEFAULT_OBJECT_WIDTH = 1.0
DEFAULT_OBJECT_HEIGHT = 1.0

CELL_SIZE = 12.0  # Slightly larger cells for better visibility
FPS = 60
WS_URL = "ws://localhost:8080/ws"
# NOTE: Adjusted interpolation time for smoother movement
INTERPOLATION_TIME_MS = 200

DEV_GUI = True
AOI_RADIUS = 15.0  # Must match server setting

# --- Colors ---
COLOR_BACKGROUND = pr.Color(30, 30, 30, 255)
COLOR_OBSTACLE = pr.Color(100, 100, 100, 255)
COLOR_PLAYER = pr.Color(0, 200, 255, 255)
COLOR_OTHER_PLAYER = pr.Color(255, 100, 0, 255)
COLOR_PATH = pr.fade(pr.GREEN, 0.5)
COLOR_TARGET = pr.Color(255, 255, 0, 255)
# NEW: AOI is now transparent purple as requested
COLOR_AOI = pr.fade(pr.PURPLE, 0.2)
COLOR_DEBUG_TEXT = pr.Color(220, 220, 220, 255)
COLOR_ERROR_TEXT = pr.Color(255, 50, 50, 255)
COLOR_PORTAL = pr.Color(180, 50, 255, 180)
COLOR_PORTAL_LABEL = pr.Color(240, 240, 240, 255)
COLOR_UI_TEXT = pr.Color(255, 255, 255, 255)
# NEW: Added a new color for the map boundary
COLOR_MAP_BOUNDARY = pr.Color(255, 255, 255, 255)


# --- Enums for Animation State ---
class Direction(Enum):
    UP = 0
    UP_RIGHT = 1
    RIGHT = 2
    DOWN_RIGHT = 3
    DOWN = 4
    DOWN_LEFT = 5
    LEFT = 6
    UP_LEFT = 7
    NONE = 8


class ObjectLayerMode(Enum):
    IDLE = 0
    WALKING = 1
    # NEW: Added TELEPORTING mode to handle transitions
    TELEPORTING = 2


# --- Game State Management ---
class GameState:
    def __init__(self):
        self.mutex = threading.Lock()
        self.player_id = None
        self.player_map_id = 0
        self.player_mode = ObjectLayerMode.IDLE
        self.player_direction = Direction.NONE
        self.grid_w = GRID_SIZE_W
        self.grid_h = GRID_SIZE_H
        self.aoi_radius = AOI_RADIUS
        self.obstacles = {}
        self.portals = {}
        self.player_pos_interpolated = pr.Vector2(0, 0)
        self.player_pos_server = pr.Vector2(0, 0)
        self.player_pos_prev = pr.Vector2(0, 0)
        self.player_dims = pr.Vector2(DEFAULT_OBJECT_WIDTH, DEFAULT_OBJECT_HEIGHT)
        self.other_players = {}
        self.path = []
        self.target_pos = pr.Vector2(-1, -1)
        self.last_update_time = time.time()
        self.last_error_message = ""
        self.error_display_time = 0.0
        self.download_size_bytes = 0
        self.upload_size_bytes = 0
        self.camera = pr.Camera2D(
            pr.Vector2(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2),
            pr.Vector2(0, 0),
            0.0,
            2.0,
        )


class NetworkClient:
    def __init__(self):
        self.game_state = GameState()
        self.ws = None
        self.ws_thread = None
        self.is_running = True
        self.mutex = self.game_state.mutex
        self.download_kbps = 0.0
        self.upload_kbps = 0.0

    def on_message(self, ws, message):
        with self.mutex:
            try:
                data = json.loads(message)
                message_type = data.get("type")
                self.game_state.download_size_bytes += len(message)

                if message_type == "init_data":
                    payload = data.get("payload", {})
                    self.game_state.grid_w = payload.get(
                        "gridW", self.game_state.grid_w
                    )
                    self.game_state.grid_h = payload.get(
                        "gridH", self.game_state.grid_h
                    )
                    self.game_state.aoi_radius = payload.get(
                        "aoiRadius", self.game_state.aoi_radius
                    )
                elif message_type == "aoi_update":
                    payload = data.get("payload")
                    if not payload:
                        return

                    # --- Player State Update ---
                    player_data = payload.get("player")
                    if player_data:
                        # Set previous position before updating to new server position
                        self.game_state.player_pos_prev = (
                            self.game_state.player_pos_interpolated
                        )

                        self.game_state.player_id = player_data.get("id")
                        self.game_state.player_map_id = player_data.get("MapID", 0)

                        try:
                            self.game_state.player_mode = ObjectLayerMode(
                                player_data.get("mode", 0)
                            )
                        except (ValueError, TypeError):
                            self.game_state.player_mode = ObjectLayerMode.IDLE

                        try:
                            self.game_state.player_direction = Direction(
                                player_data.get("direction", 8)  # 8 is NONE
                            )
                        except (ValueError, TypeError):
                            self.game_state.player_direction = Direction.NONE

                        pos = player_data.get("Pos", {})
                        self.game_state.player_pos_server = pr.Vector2(
                            pos.get("X", 0.0), pos.get("Y", 0.0)
                        )

                        # NEW: Check if the player is teleporting to prevent interpolation jump
                        if self.game_state.player_mode == ObjectLayerMode.TELEPORTING:
                            self.game_state.player_pos_interpolated = (
                                self.game_state.player_pos_server
                            )
                            self.game_state.player_pos_prev = (
                                self.game_state.player_pos_server
                            )

                        self.game_state.last_update_time = time.time()

                        dims = player_data.get("Dims", {})
                        self.game_state.player_dims = pr.Vector2(
                            dims.get("Width", 1.0), dims.get("Height", 1.0)
                        )

                        path_data = player_data.get("path")
                        if path_data is not None:
                            self.game_state.path = [
                                pr.Vector2(p.get("X"), p.get("Y")) for p in path_data
                            ]
                        else:
                            self.game_state.path = []

                        target_pos_data = player_data.get("targetPos")
                        if target_pos_data:
                            self.game_state.target_pos = pr.Vector2(
                                target_pos_data.get("X"), target_pos_data.get("Y")
                            )
                        else:
                            self.game_state.target_pos = pr.Vector2(-1, -1)

                    # --- Other Players Update ---
                    visible_players_data = payload.get("visiblePlayers")
                    self.game_state.other_players = {}
                    if visible_players_data:
                        for player_id, p_data in visible_players_data.items():
                            if player_id != self.game_state.player_id:
                                pos = p_data.get("Pos", {})
                                dims = p_data.get("Dims", {})
                                self.game_state.other_players[player_id] = {
                                    "pos": pr.Vector2(pos.get("X"), pos.get("Y")),
                                    "dims": pr.Vector2(
                                        dims.get("Width"), dims.get("Height")
                                    ),
                                }

                    # --- Grid Objects (Obstacles/Portals) Update ---
                    visible_objects_data = payload.get("visibleGridObjects")
                    # Clear and update only the visible objects to improve performance
                    self.game_state.obstacles = {}
                    self.game_state.portals = {}
                    if visible_objects_data:
                        for obj_id, obj_data in visible_objects_data.items():
                            obj_type = obj_data.get("Type")
                            pos = obj_data.get("Pos", {})
                            dims = obj_data.get("Dims", {})
                            if obj_type == "obstacle":
                                self.game_state.obstacles[obj_id] = {
                                    "pos": pr.Vector2(pos.get("X"), pos.get("Y")),
                                    "dims": pr.Vector2(
                                        dims.get("Width"), dims.get("Height")
                                    ),
                                }
                            elif obj_type == "portal":
                                self.game_state.portals[obj_id] = {
                                    "pos": pr.Vector2(pos.get("X"), pos.get("Y")),
                                    "dims": pr.Vector2(
                                        dims.get("Width"), dims.get("Height")
                                    ),
                                    "label": obj_data.get("PortalLabel"),
                                }

            except json.JSONDecodeError as e:
                with self.mutex:
                    self.game_state.last_error_message = f"JSON Decode Error: {e}"
                    self.game_state.error_display_time = time.time()
                print(f"JSON Decode Error: {e}")
            except Exception as e:
                with self.mutex:
                    self.game_state.last_error_message = f"Error: {e}"
                    self.game_state.error_display_time = time.time()
                print(f"Error: {e}")

    def on_error(self, ws, error):
        with self.mutex:
            self.game_state.last_error_message = f"WebSocket Error: {error}"
            self.game_state.error_display_time = time.time()
        print(f"### WebSocket Error ###: {error}")

    def on_close(self, ws, close_status_code, close_msg):
        with self.mutex:
            self.game_state.last_error_message = (
                f"WebSocket Closed: {close_status_code}, {close_msg}"
            )
            self.game_state.error_display_time = time.time()
        print("### WebSocket Closed ###")
        self.ws = None

    def on_open(self, ws):
        print("WebSocket Opened. Sending join request...")
        try:
            join_message = {"type": "join_request", "payload": {}}
            self.ws.send(json.dumps(join_message))
        except Exception as e:
            print(f"Error sending join request: {e}")

    def run_websocket_thread(self):
        self.ws = websocket.WebSocketApp(
            WS_URL,
            on_open=self.on_open,
            on_message=self.on_message,
            on_error=self.on_error,
            on_close=self.on_close,
        )
        self.ws.run_forever(reconnect=5)

    def start(self):
        print("Starting WebSocket client...")
        self.ws_thread = threading.Thread(target=self.run_websocket_thread, daemon=True)
        self.ws_thread.start()
        self.run_game_loop()

    def send_player_action(self, target_x, target_y):
        if self.ws and self.ws.sock and self.ws.sock.connected:
            try:
                action_message = {
                    "type": "player_action",
                    "payload": {"targetX": target_x, "targetY": target_y},
                }
                self.ws.send(json.dumps(action_message))
                self.game_state.upload_size_bytes += len(json.dumps(action_message))
            except websocket.WebSocketConnectionClosedException:
                print("Cannot send message, connection is closed.")
            except Exception as e:
                print(f"Error sending message: {e}")

    def interpolate_player_position(self):
        with self.mutex:
            # Calculate the elapsed time since the last server update
            time_since_update = time.time() - self.game_state.last_update_time
            # Calculate the interpolation factor (clamped between 0 and 1)
            # This determines how far along the interpolation we are
            interp_factor = min(
                1.0, time_since_update / (INTERPOLATION_TIME_MS / 1000.0)
            )

            # Interpolate the player's position between the previous and server positions
            # This creates smooth movement instead of "snapping" to the new position
            current_x = pr.lerp(
                self.game_state.player_pos_prev.x,
                self.game_state.player_pos_server.x,
                interp_factor,
            )
            current_y = pr.lerp(
                self.game_state.player_pos_prev.y,
                self.game_state.player_pos_server.y,
                interp_factor,
            )
            self.game_state.player_pos_interpolated = pr.Vector2(current_x, current_y)

    def draw_grid_lines(self):
        grid_w, grid_h = self.game_state.grid_w, self.game_state.grid_h
        cell_size = CELL_SIZE
        map_w, map_h = grid_w * cell_size, grid_h * cell_size
        pr.draw_rectangle_lines_ex(
            pr.Rectangle(0, 0, map_w, map_h), 1, COLOR_MAP_BOUNDARY
        )
        for i in range(grid_w):
            start_pos = pr.Vector2(i * cell_size, 0)
            end_pos = pr.Vector2(i * cell_size, map_h)
            pr.draw_line_ex(start_pos, end_pos, 1, pr.fade(COLOR_MAP_BOUNDARY, 0.2))
        for j in range(grid_h):
            start_pos = pr.Vector2(0, j * cell_size)
            end_pos = pr.Vector2(map_w, j * cell_size)
            pr.draw_line_ex(start_pos, end_pos, 1, pr.fade(COLOR_MAP_BOUNDARY, 0.2))

    def draw_player(self):
        player_dims = self.game_state.player_dims
        player_pos = self.game_state.player_pos_interpolated
        scaled_pos_x = player_pos.x * CELL_SIZE
        scaled_pos_y = player_pos.y * CELL_SIZE
        scaled_dims_w = player_dims.x * CELL_SIZE
        scaled_dims_h = player_dims.y * CELL_SIZE
        pr.draw_rectangle_pro(
            pr.Rectangle(scaled_pos_x, scaled_pos_y, scaled_dims_w, scaled_dims_h),
            pr.Vector2(0, 0),
            0,
            COLOR_PLAYER,
        )

    def draw_other_players(self):
        with self.mutex:
            for player_id, player_data in self.game_state.other_players.items():
                pos = player_data["pos"]
                dims = player_data["dims"]
                scaled_pos_x = pos.x * CELL_SIZE
                scaled_pos_y = pos.y * CELL_SIZE
                scaled_dims_w = dims.x * CELL_SIZE
                scaled_dims_h = dims.y * CELL_SIZE
                pr.draw_rectangle_pro(
                    pr.Rectangle(
                        scaled_pos_x, scaled_pos_y, scaled_dims_w, scaled_dims_h
                    ),
                    pr.Vector2(0, 0),
                    0,
                    COLOR_OTHER_PLAYER,
                )

    def draw_grid_objects(self):
        cell_size = CELL_SIZE
        with self.mutex:
            # Draw obstacles
            for obj_id, obj_data in self.game_state.obstacles.items():
                pos = obj_data["pos"]
                dims = obj_data["dims"]
                pr.draw_rectangle_pro(
                    pr.Rectangle(
                        pos.x * cell_size,
                        pos.y * cell_size,
                        dims.x * cell_size,
                        dims.y * cell_size,
                    ),
                    pr.Vector2(0, 0),
                    0,
                    COLOR_OBSTACLE,
                )

            # Draw portals with labels
            for portal_id, portal_data in self.game_state.portals.items():
                pos = portal_data["pos"]
                dims = portal_data["dims"]
                label = portal_data["label"]
                pr.draw_rectangle_pro(
                    pr.Rectangle(
                        pos.x * cell_size,
                        pos.y * cell_size,
                        dims.x * cell_size,
                        dims.y * cell_size,
                    ),
                    pr.Vector2(0, 0),
                    0,
                    COLOR_PORTAL,
                )
                label_pos = pr.Vector2(
                    (pos.x + dims.x / 2) * cell_size,
                    (pos.y + dims.y / 2) * cell_size,
                )
                pr.draw_text_pro(
                    pr.get_font_default(),
                    label,
                    label_pos,
                    pr.Vector2(
                        pr.measure_text(label, 10) / 2, 5
                    ),  # Centered text origin
                    0,
                    10,
                    2,
                    COLOR_PORTAL_LABEL,
                )

    def draw_path(self):
        with self.mutex:
            if self.game_state.path:
                cell_size = CELL_SIZE
                # Draw the target position
                target_x, target_y = (
                    self.game_state.target_pos.x,
                    self.game_state.target_pos.y,
                )
                if target_x >= 0 and target_y >= 0:
                    pr.draw_rectangle_pro(
                        pr.Rectangle(
                            target_x * cell_size,
                            target_y * cell_size,
                            CELL_SIZE,
                            CELL_SIZE,
                        ),
                        pr.Vector2(0, 0),
                        0,
                        COLOR_TARGET,
                    )
                # Draw the path
                for p in self.game_state.path:
                    pr.draw_rectangle_pro(
                        pr.Rectangle(
                            p.x * cell_size, p.y * cell_size, CELL_SIZE, CELL_SIZE
                        ),
                        pr.Vector2(0, 0),
                        0,
                        COLOR_PATH,
                    )

    def draw_aoi_circle(self):
        with self.mutex:
            player_pos = self.game_state.player_pos_interpolated
            pr.draw_circle_v(
                pr.Vector2(player_pos.x * CELL_SIZE, player_pos.y * CELL_SIZE),
                self.game_state.aoi_radius * CELL_SIZE,
                COLOR_AOI,
            )

    def draw_dev_ui(self):
        if not DEV_GUI:
            return

        pr.draw_rectangle(0, 0, SCREEN_WIDTH, 120, pr.fade(pr.BLACK, 0.7))
        pr.draw_text_ex(
            pr.get_font_default(),
            "DEV UI",
            pr.Vector2(10, 10),
            20,
            1,
            COLOR_DEBUG_TEXT,
        )
        with self.mutex:
            player_id = (
                self.game_state.player_id if self.game_state.player_id else "N/A"
            )
            player_map_id = self.game_state.player_map_id
            player_mode = self.game_state.player_mode.name
            player_dir = self.game_state.player_direction.name
            target_pos = self.game_state.target_pos
            download_kbps = self.download_kbps
            upload_kbps = self.upload_kbps
            error_msg = self.game_state.last_error_message

            # Use interpolated position for the UI
            player_pos_ui = self.game_state.player_pos_interpolated

            text_lines = [
                f"Player ID: {player_id}",
                f"Map ID: {player_map_id}",
                f"Mode: {player_mode} | Direction: {player_dir}",
                f"Pos: ({player_pos_ui.x:.2f}, {player_pos_ui.y:.2f})",  # Use interpolated position here
                f"Target: ({target_pos.x:.0f}, {target_pos.y:.0f})",
                f"Download: {download_kbps:.2f} kbps | Upload: {upload_kbps:.2f} kbps",
            ]

            y_offset = 30
            for line in text_lines:
                pr.draw_text_ex(
                    pr.get_font_default(),
                    line,
                    pr.Vector2(10, y_offset),
                    18,
                    1,
                    COLOR_UI_TEXT,
                )
                y_offset += 20

            if error_msg:
                pr.draw_text_ex(
                    pr.get_font_default(),
                    f"Error: {error_msg}",
                    pr.Vector2(10, SCREEN_HEIGHT - 30),
                    18,
                    1,
                    COLOR_ERROR_TEXT,
                )

    def draw_game(self):
        pr.begin_drawing()
        pr.clear_background(COLOR_BACKGROUND)

        with self.mutex:
            self.game_state.camera.target = pr.Vector2(
                self.game_state.player_pos_interpolated.x * CELL_SIZE,
                self.game_state.player_pos_interpolated.y * CELL_SIZE,
            )

        pr.begin_mode_2d(self.game_state.camera)

        # Draw game elements in world space
        self.draw_grid_lines()
        self.draw_grid_objects()
        self.draw_other_players()
        self.draw_player()
        self.draw_path()
        self.draw_aoi_circle()

        pr.end_mode_2d()

        # Draw UI elements in screen space
        self.draw_dev_ui()

        pr.end_drawing()

    def run_game_loop(self):
        pr.set_config_flags(pr.ConfigFlags.FLAG_VSYNC_HINT)
        pr.init_window(SCREEN_WIDTH, SCREEN_HEIGHT, "MMO Client")
        pr.set_target_fps(FPS)

        last_download_check_time = time.time()
        while not pr.window_should_close() and self.is_running:
            # Check for mouse click to move player
            if pr.is_mouse_button_pressed(pr.MOUSE_LEFT_BUTTON):
                mouse_pos = pr.get_mouse_position()
                world_pos = pr.get_screen_to_world_2d(mouse_pos, self.game_state.camera)
                target_x = world_pos.x / CELL_SIZE
                target_y = world_pos.y / CELL_SIZE

                # Snap to grid
                target_x = math.floor(target_x)
                target_y = math.floor(target_y)

                self.send_player_action(target_x, target_y)

            # Update download/upload rates
            current_time = time.time()
            if current_time - last_download_check_time >= 1:
                with self.mutex:
                    self.download_kbps = (
                        self.game_state.download_size_bytes / 1024
                    ) * 8
                    self.upload_kbps = (self.game_state.upload_size_bytes / 1024) * 8
                    self.game_state.download_size_bytes = 0
                    self.game_state.upload_size_bytes = 0
                last_download_check_time = current_time

            self.interpolate_player_position()
            self.draw_game()

        print("Closing WebSocket...")
        if self.ws:
            self.ws.close()
        self.is_running = False
        pr.close_window()


if __name__ == "__main__":
    client = NetworkClient()
    client.start()
