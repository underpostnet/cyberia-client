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
                                    "label": obj_data.get("PortalLabel", ""),
                                }

            except json.JSONDecodeError as e:
                self.set_error_message(f"JSON decode error: {e}")
            except Exception as e:
                self.set_error_message(f"An error occurred: {e}")

    def on_error(self, ws, error):
        self.set_error_message(f"WebSocket Error: {error}")

    def on_close(self, ws, close_status_code, close_msg):
        self.set_error_message(f"WebSocket closed: {close_msg} ({close_status_code})")

    def on_open(self, ws):
        print("WebSocket connection opened.")

    def set_error_message(self, message):
        with self.mutex:
            self.game_state.last_error_message = message
            self.game_state.error_display_time = time.time()

    def connect(self):
        self.ws = websocket.WebSocketApp(
            WS_URL,
            on_open=self.on_open,
            on_message=self.on_message,
            on_error=self.on_error,
            on_close=self.on_close,
        )
        self.ws_thread = threading.Thread(target=self.ws.run_forever)
        self.ws_thread.daemon = True
        self.ws_thread.start()

    def send_player_action(self, target_x, target_y):
        if self.ws:
            action = {
                "type": "player_action",
                "payload": {"targetX": target_x, "targetY": target_y},
            }
            try:
                message = json.dumps(action)
                self.ws.send(message)
                self.game_state.upload_size_bytes += len(message)
            except Exception as e:
                self.set_error_message(f"Failed to send message: {e}")

    def interpolate_player_position(self):
        with self.mutex:
            elapsed_time = (time.time() - self.game_state.last_update_time) * 1000
            t = min(elapsed_time / INTERPOLATION_TIME_MS, 1.0)

            # Only interpolate if the player is not teleporting
            if self.game_state.player_mode != ObjectLayerMode.TELEPORTING:
                self.game_state.player_pos_interpolated.x = pr.lerp(
                    self.game_state.player_pos_prev.x,
                    self.game_state.player_pos_server.x,
                    t,
                )
                self.game_state.player_pos_interpolated.y = pr.lerp(
                    self.game_state.player_pos_prev.y,
                    self.game_state.player_pos_server.y,
                    t,
                )
            else:
                # When teleporting, the position is already set to the server position
                # No interpolation needed.
                pass

    def draw_game(self):
        with self.mutex:
            pr.begin_drawing()
            pr.clear_background(COLOR_BACKGROUND)
            pr.begin_mode_2d(self.game_state.camera)

            # NEW: Draw the map boundary
            pr.draw_rectangle_lines_ex(
                pr.Rectangle(
                    0,
                    0,
                    self.game_state.grid_w * CELL_SIZE,
                    self.game_state.grid_h * CELL_SIZE,
                ),
                2.0,  # Line thickness
                COLOR_MAP_BOUNDARY,
            )

            # Draw Obstacles (newly added)
            for obs_id, obs_data in self.game_state.obstacles.items():
                pos = obs_data["pos"]
                dims = obs_data["dims"]
                pr.draw_rectangle(
                    int(pos.x * CELL_SIZE),
                    int(pos.y * CELL_SIZE),
                    int(dims.x * CELL_SIZE),
                    int(dims.y * CELL_SIZE),
                    COLOR_OBSTACLE,
                )

            # Draw Portals
            for portal_id, portal_data in self.game_state.portals.items():
                pos = portal_data["pos"]
                dims = portal_data["dims"]
                label = portal_data["label"]
                pr.draw_rectangle(
                    int(pos.x * CELL_SIZE),
                    int(pos.y * CELL_SIZE),
                    int(dims.x * CELL_SIZE),
                    int(dims.y * CELL_SIZE),
                    COLOR_PORTAL,
                )
                text_width = pr.measure_text(label, 10)
                pr.draw_text(
                    label,
                    int((pos.x + dims.x / 2) * CELL_SIZE - text_width / 2),
                    int((pos.y + dims.y / 2) * CELL_SIZE - 5),
                    10,
                    COLOR_PORTAL_LABEL,
                )

            # Draw Path
            if len(self.game_state.path) > 1:
                for i in range(len(self.game_state.path) - 1):
                    p1 = pr.Vector2(
                        (self.game_state.path[i].x + 0.5) * CELL_SIZE,
                        (self.game_state.path[i].y + 0.5) * CELL_SIZE,
                    )
                    p2 = pr.Vector2(
                        (self.game_state.path[i + 1].x + 0.5) * CELL_SIZE,
                        (self.game_state.path[i + 1].y + 0.5) * CELL_SIZE,
                    )
                    pr.draw_line_ex(p1, p2, 2, COLOR_PATH)

            # Draw Player
            player_pos = self.game_state.player_pos_interpolated
            pr.draw_rectangle_pro(
                pr.Rectangle(
                    (player_pos.x + self.game_state.player_dims.x / 2) * CELL_SIZE,
                    (player_pos.y + self.game_state.player_dims.y / 2) * CELL_SIZE,
                    self.game_state.player_dims.x * CELL_SIZE,
                    self.game_state.player_dims.y * CELL_SIZE,
                ),
                pr.Vector2(
                    self.game_state.player_dims.x / 2 * CELL_SIZE,
                    self.game_state.player_dims.y / 2 * CELL_SIZE,
                ),
                0,
                COLOR_PLAYER,
            )

            # Draw AOI Circle
            pr.draw_circle_v(
                pr.Vector2(
                    (player_pos.x + self.game_state.player_dims.x / 2) * CELL_SIZE,
                    (player_pos.y + self.game_state.player_dims.y / 2) * CELL_SIZE,
                ),
                self.game_state.aoi_radius * CELL_SIZE,
                COLOR_AOI,
            )

            # Draw other players
            for p_id, p_data in self.game_state.other_players.items():
                pos = p_data["pos"]
                dims = p_data["dims"]
                pr.draw_rectangle_pro(
                    pr.Rectangle(
                        (pos.x + dims.x / 2) * CELL_SIZE,
                        (pos.y + dims.y / 2) * CELL_SIZE,
                        dims.x * CELL_SIZE,
                        dims.y * CELL_SIZE,
                    ),
                    pr.Vector2(dims.x / 2 * CELL_SIZE, dims.y / 2 * CELL_SIZE),
                    0,
                    COLOR_OTHER_PLAYER,
                )

            # Update camera to follow player smoothly
            self.game_state.camera.target = pr.Vector2(
                (player_pos.x + self.game_state.player_dims.x / 2) * CELL_SIZE,
                (player_pos.y + self.game_state.player_dims.y / 2) * CELL_SIZE,
            )

            pr.end_mode_2d()

            # Draw UI and debug info
            self.draw_ui()

            pr.end_drawing()

    def draw_ui(self):
        # UI elements are drawn in screen space (not affected by camera)
        pr.draw_fps(10, 10)

        # Dev GUI
        if DEV_GUI:
            pr.draw_text(
                f"Player ID: {self.game_state.player_id}",
                10,
                30,
                10,
                COLOR_DEBUG_TEXT,
            )
            pr.draw_text(
                f"Map ID: {self.game_state.player_map_id}",
                10,
                50,
                10,
                COLOR_DEBUG_TEXT,
            )
            pr.draw_text(
                f"Pos: ({self.game_state.player_pos_server.x:.2f}, {self.game_state.player_pos_server.y:.2f})",
                10,
                70,
                10,
                COLOR_DEBUG_TEXT,
            )
            pr.draw_text(
                f"Interp Pos: ({self.game_state.player_pos_interpolated.x:.2f}, {self.game_state.player_pos_interpolated.y:.2f})",
                10,
                90,
                10,
                COLOR_DEBUG_TEXT,
            )
            pr.draw_text(
                f"Mode: {self.game_state.player_mode.name}",
                10,
                110,
                10,
                COLOR_DEBUG_TEXT,
            )
            pr.draw_text(
                f"Dir: {self.game_state.player_direction.name}",
                10,
                130,
                10,
                COLOR_DEBUG_TEXT,
            )
            pr.draw_text(
                f"Path Len: {len(self.game_state.path)}",
                10,
                150,
                10,
                COLOR_DEBUG_TEXT,
            )
            pr.draw_text(
                f"Obstacles: {len(self.game_state.obstacles)}",
                10,
                170,
                10,
                COLOR_DEBUG_TEXT,
            )
            pr.draw_text(
                f"Other Players: {len(self.game_state.other_players)}",
                10,
                190,
                10,
                COLOR_DEBUG_TEXT,
            )
            pr.draw_text(
                f"Download: {self.download_kbps:.2f} kbps",
                10,
                210,
                10,
                COLOR_DEBUG_TEXT,
            )
            pr.draw_text(
                f"Upload: {self.upload_kbps:.2f} kbps",
                10,
                230,
                10,
                COLOR_DEBUG_TEXT,
            )

        # Draw error message if present
        if time.time() - self.game_state.error_display_time < 5.0:
            text_size = pr.measure_text(self.game_state.last_error_message, 20)
            pr.draw_text(
                self.game_state.last_error_message,
                (SCREEN_WIDTH - text_size) // 2,
                SCREEN_HEIGHT - 30,
                20,
                COLOR_ERROR_TEXT,
            )

    def run(self):
        pr.set_config_flags(pr.ConfigFlags.FLAG_VSYNC_HINT)
        pr.set_target_fps(FPS)
        pr.init_window(SCREEN_WIDTH, SCREEN_HEIGHT, "MMO Game Client")
        self.connect()

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
        pr.close_window()


if __name__ == "__main__":
    client = NetworkClient()
    client.run()
