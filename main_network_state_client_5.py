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
INTERPOLATION_TIME_MS = 50

DEV_GUI = True
AOI_RADIUS = 25.0

# --- Colors ---
COLOR_BACKGROUND = pr.Color(30, 30, 30, 255)
COLOR_OBSTACLE = pr.Color(100, 100, 100, 255)
COLOR_PLAYER = pr.Color(0, 200, 255, 255)
COLOR_OTHER_PLAYER = pr.Color(255, 100, 0, 255)
COLOR_PATH = pr.fade(pr.GREEN, 0.5)
COLOR_TARGET = pr.Color(255, 255, 0, 255)
COLOR_AOI = pr.fade(pr.PURPLE, 0.2)
COLOR_DEBUG_TEXT = pr.Color(220, 220, 220, 255)
COLOR_ERROR_TEXT = pr.Color(255, 50, 50, 255)
COLOR_PORTAL = pr.Color(180, 50, 255, 180)
COLOR_PORTAL_LABEL = pr.Color(240, 240, 240, 255)


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


# --- Game State Management ---
class GameState:
    def __init__(self):
        self.mutex = threading.Lock()
        self.player_id = None
        self.player_map_id = 0
        self.player_mode = ObjectLayerMode.IDLE
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
        self.aoi_rect = pr.Rectangle(0, 0, 0, 0)
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
                    self.game_state.obstacles = (
                        {}
                    )  # Obstacles now arrive in AOI updates

                elif message_type == "aoi_update":
                    payload = data.get("payload")
                    if not payload:
                        return

                    # --- Player State Update ---
                    player_data = payload.get("player")
                    if player_data:
                        self.game_state.player_id = player_data.get("id")
                        self.game_state.player_map_id = player_data.get("MapID", 0)

                        try:
                            self.game_state.player_mode = ObjectLayerMode(
                                player_data.get("Mode", 0)
                            )
                        except (ValueError, TypeError):
                            self.game_state.player_mode = ObjectLayerMode.IDLE

                        pos = player_data.get("Pos", {})
                        self.game_state.player_pos_prev = (
                            self.game_state.player_pos_server
                        )
                        self.game_state.player_pos_server = pr.Vector2(
                            pos.get("X", 0.0), pos.get("Y", 0.0)
                        )
                        self.game_state.last_update_time = time.time()

                        dims = player_data.get("Dims", {})
                        self.game_state.player_dims = pr.Vector2(
                            dims.get("Width", 1.0), dims.get("Height", 1.0)
                        )

                        # FIX: Safely handle path updates
                        path_data = player_data.get("path")
                        self.game_state.path = (
                            [pr.Vector2(p.get("X"), p.get("Y")) for p in path_data]
                            if path_data is not None
                            else []
                        )

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
                    self.game_state.obstacles = {}
                    self.game_state.portals = {}
                    if visible_objects_data:
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
                                # NEW: Store portal label
                                obj["label"] = obj_data.get("PortalLabel", "???")
                                self.game_state.portals[obj_id] = obj

            except (json.JSONDecodeError, KeyError, TypeError) as e:
                self.game_state.last_error_message = f"Processing Error: {e}"
                self.game_state.error_display_time = time.time()
                print(f"Error processing message: {message[:200]}... | Exception: {e}")

    def on_error(self, ws, error):
        print(f"### WebSocket Error ###: {error}")

    def on_close(self, ws, close_status_code, close_msg):
        print(f"### WebSocket Closed ###: {close_status_code} - {close_msg}")
        self.is_running = False

    def on_open(self, ws):
        print("WebSocket connection opened.")

    def run_websocket_thread(self):
        self.ws = websocket.WebSocketApp(
            WS_URL,
            on_open=self.on_open,
            on_message=self.on_message,
            on_error=self.on_error,
            on_close=self.on_close,
        )
        self.ws.run_forever(ping_interval=20, ping_timeout=10)

    def start(self):
        pr.init_window(SCREEN_WIDTH, SCREEN_HEIGHT, "Networked Game Client")
        pr.set_target_fps(FPS)
        if not pr.is_window_ready():
            return

        self.ws_thread = threading.Thread(target=self.run_websocket_thread, daemon=True)
        self.ws_thread.start()
        self.run_game_loop()

    def draw_grid(self):
        for x in range(0, self.game_state.grid_w + 1, 5):
            pr.draw_line(
                int(x * CELL_SIZE),
                0,
                int(x * CELL_SIZE),
                int(self.game_state.grid_h * CELL_SIZE),
                pr.fade(pr.LIGHTGRAY, 0.1),
            )
        for y in range(0, self.game_state.grid_h + 1, 5):
            pr.draw_line(
                0,
                int(y * CELL_SIZE),
                int(self.game_state.grid_w * CELL_SIZE),
                int(y * CELL_SIZE),
                pr.fade(pr.LIGHTGRAY, 0.1),
            )

    def draw_portals(self):
        for portal_id, portal_data in self.game_state.portals.items():
            pos, dims = portal_data["pos"], portal_data["dims"]
            rect = pr.Rectangle(
                pos.x * CELL_SIZE,
                pos.y * CELL_SIZE,
                dims.x * CELL_SIZE,
                dims.y * CELL_SIZE,
            )
            pr.draw_rectangle_rec(rect, COLOR_PORTAL)

            # NEW: Draw portal label
            label = portal_data.get("label", "")
            text_width = pr.measure_text(label, 10)
            pr.draw_text(
                label,
                int(rect.x + (rect.width / 2) - (text_width / 2)),
                int(rect.y - 12),
                10,
                COLOR_PORTAL_LABEL,
            )

    def draw_game_objects(self):
        for _, data in self.game_state.obstacles.items():
            pr.draw_rectangle_rec(
                pr.Rectangle(
                    data["pos"].x * CELL_SIZE,
                    data["pos"].y * CELL_SIZE,
                    data["dims"].x * CELL_SIZE,
                    data["dims"].y * CELL_SIZE,
                ),
                COLOR_OBSTACLE,
            )

        for _, data in self.game_state.other_players.items():
            pr.draw_rectangle_rec(
                pr.Rectangle(
                    data["pos"].x * CELL_SIZE,
                    data["pos"].y * CELL_SIZE,
                    data["dims"].x * CELL_SIZE,
                    data["dims"].y * CELL_SIZE,
                ),
                COLOR_OTHER_PLAYER,
            )

        pos, dims = self.game_state.player_pos_interpolated, self.game_state.player_dims
        pr.draw_rectangle_rec(
            pr.Rectangle(
                pos.x * CELL_SIZE,
                pos.y * CELL_SIZE,
                dims.x * CELL_SIZE,
                dims.y * CELL_SIZE,
            ),
            COLOR_PLAYER,
        )

    def draw_debug_info(self, fps, download_kbps, upload_kbps):
        if not DEV_GUI:
            return

        # NEW: Display current Map ID and Player Mode
        pr.draw_text(f"FPS: {fps}", 10, 10, 20, COLOR_DEBUG_TEXT)
        pr.draw_text(
            f"Map: {self.game_state.player_map_id}", 10, 35, 20, COLOR_DEBUG_TEXT
        )
        pr.draw_text(
            f"Mode: {self.game_state.player_mode.name}", 10, 60, 20, COLOR_DEBUG_TEXT
        )
        pr.draw_text(f"DL: {download_kbps:.2f} kbps", 10, 85, 20, COLOR_DEBUG_TEXT)
        pr.draw_text(f"UL: {upload_kbps:.2f} kbps", 10, 110, 20, COLOR_DEBUG_TEXT)

        if time.time() - self.game_state.error_display_time < 5.0:
            pr.draw_text(
                self.game_state.last_error_message,
                10,
                pr.get_screen_height() - 30,
                20,
                COLOR_ERROR_TEXT,
            )

    def send_path_request(self, grid_x, grid_y):
        if self.ws and self.ws.sock and self.ws.sock.connected:
            message = {
                "type": "path_request",
                "payload": {"targetPos": {"X": float(grid_x), "Y": float(grid_y)}},
            }
            try:
                json_message = json.dumps(message)
                self.game_state.upload_size_bytes += len(json_message)
                self.ws.send(json_message)
            except Exception as e:
                self.game_state.last_error_message = f"Send failed: {e}"
                self.game_state.error_display_time = time.time()

    def run_game_loop(self):
        last_kbps_update = time.time()
        while not pr.window_should_close() and self.is_running:
            current_time = time.time()
            if current_time - last_kbps_update >= 1.0:
                self.download_kbps = self.game_state.download_size_bytes * 8 / 1024
                self.upload_kbps = self.game_state.upload_size_bytes * 8 / 1024
                self.game_state.download_size_bytes = 0
                self.game_state.upload_size_bytes = 0
                last_kbps_update = current_time

            with self.mutex:
                factor = min(
                    ((current_time - self.game_state.last_update_time) * 1000)
                    / INTERPOLATION_TIME_MS,
                    1.0,
                )
                self.game_state.player_pos_interpolated = pr.vector2_lerp(
                    self.game_state.player_pos_prev,
                    self.game_state.player_pos_server,
                    factor,
                )

                cam_target_x = (
                    self.game_state.player_pos_interpolated.x * CELL_SIZE
                    + self.game_state.player_dims.x * CELL_SIZE / 2
                )
                cam_target_y = (
                    self.game_state.player_pos_interpolated.y * CELL_SIZE
                    + self.game_state.player_dims.y * CELL_SIZE / 2
                )
                self.game_state.camera.target = pr.Vector2(cam_target_x, cam_target_y)

            if pr.is_mouse_button_pressed(pr.MOUSE_BUTTON_LEFT):
                world_pos = pr.get_screen_to_world_2d(
                    pr.get_mouse_position(), self.game_state.camera
                )
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
            with self.mutex:
                self.draw_grid()
                self.draw_portals()
                self.draw_game_objects()
            pr.end_mode_2d()
            with self.mutex:
                self.draw_debug_info(pr.get_fps(), self.download_kbps, self.upload_kbps)
            pr.end_drawing()

        print("Closing WebSocket...")
        if self.ws:
            self.ws.close()
        if self.ws_thread:
            self.ws_thread.join(timeout=2)
        pr.close_window()


if __name__ == "__main__":
    client = NetworkClient()
    client.start()
