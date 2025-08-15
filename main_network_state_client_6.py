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
INTERPOLATION_TIME_MS = 100  # Increased for smoother transitions over network latency

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
                                obj["label"] = obj_data.get("PortalLabel", "")
                                self.game_state.portals[obj_id] = obj

            except json.JSONDecodeError as e:
                self.game_state.last_error_message = f"JSON Error: {e}"
                self.game_state.error_display_time = time.time()
            except Exception as e:
                self.game_state.last_error_message = f"Error: {e}"
                self.game_state.error_display_time = time.time()

    def on_error(self, ws, error):
        with self.mutex:
            self.game_state.last_error_message = f"WS Error: {error}"
            self.game_state.error_display_time = time.time()
            print(f"WS Error: {error}")

    def on_close(self, ws, close_status_code, close_msg):
        print(f"### closed ### Status: {close_status_code}, Message: {close_msg}")
        self.is_running = False

    def on_open(self, ws):
        print("Opened connection")
        self.is_running = True

    def connect(self):
        self.ws = websocket.WebSocketApp(
            WS_URL,
            on_open=self.on_open,
            on_message=self.on_message,
            on_error=self.on_error,
            on_close=self.on_close,
        )
        self.ws_thread = threading.Thread(
            target=self.ws.run_forever,
            kwargs={"dispatcher": None, "reconnect": 5},
            daemon=True,
        )
        self.ws_thread.start()

    def disconnect(self):
        if self.ws:
            self.ws.close()
        if self.ws_thread:
            self.ws_thread.join()

    def send_path_request(self, target_x, target_y):
        payload = {
            "type": "path_request",
            "payload": {"targetX": target_x, "targetY": target_y},
        }
        self.ws.send(json.dumps(payload))
        self.game_state.upload_size_bytes += len(json.dumps(payload))

    def update_camera(self):
        # Update camera position based on the interpolated player position
        self.game_state.camera.target = pr.Vector2(
            self.game_state.player_pos_interpolated.x * CELL_SIZE
            + self.game_state.player_dims.x * CELL_SIZE / 2,
            self.game_state.player_pos_interpolated.y * CELL_SIZE
            + self.game_state.player_dims.y * CELL_SIZE / 2,
        )

    def draw_grid(self):
        # Only draw the visible grid lines to improve performance
        start_x = max(
            0,
            int(
                self.game_state.camera.target.x / CELL_SIZE
                - SCREEN_WIDTH / (2 * CELL_SIZE * self.game_state.camera.zoom)
            )
            - 1,
        )
        end_x = min(
            self.game_state.grid_w,
            int(
                self.game_state.camera.target.x / CELL_SIZE
                + SCREEN_WIDTH / (2 * CELL_SIZE * self.game_state.camera.zoom)
            )
            + 1,
        )
        start_y = max(
            0,
            int(
                self.game_state.camera.target.y / CELL_SIZE
                - SCREEN_HEIGHT / (2 * CELL_SIZE * self.game_state.camera.zoom)
            )
            - 1,
        )
        end_y = min(
            self.game_state.grid_h,
            int(
                self.game_state.camera.target.y / CELL_SIZE
                + SCREEN_HEIGHT / (2 * CELL_SIZE * self.game_state.camera.zoom)
            )
            + 1,
        )

        for y in range(start_y, end_y):
            pr.draw_line(
                int(start_x * CELL_SIZE),
                int(y * CELL_SIZE),
                int(end_x * CELL_SIZE),
                int(y * CELL_SIZE),
                pr.fade(pr.DARKGRAY, 0.5),
            )
        for x in range(start_x, end_x):
            pr.draw_line(
                int(x * CELL_SIZE),
                int(start_y * CELL_SIZE),
                int(x * CELL_SIZE),
                int(end_y * CELL_SIZE),
                pr.fade(pr.DARKGRAY, 0.5),
            )

    def draw_obstacles(self):
        for obj_id, obj in self.game_state.obstacles.items():
            pos_x = obj["pos"].x * CELL_SIZE
            pos_y = obj["pos"].y * CELL_SIZE
            width = obj["dims"].x * CELL_SIZE
            height = obj["dims"].y * CELL_SIZE
            pr.draw_rectangle(
                int(pos_x), int(pos_y), int(width), int(height), COLOR_OBSTACLE
            )

    def draw_portals(self):
        for portal_id, portal in self.game_state.portals.items():
            pos_x = portal["pos"].x * CELL_SIZE
            pos_y = portal["pos"].y * CELL_SIZE
            width = portal["dims"].x * CELL_SIZE
            height = portal["dims"].y * CELL_SIZE
            pr.draw_rectangle(
                int(pos_x), int(pos_y), int(width), int(height), COLOR_PORTAL
            )

            label = portal.get("label", "Portal")
            label_pos_x = int(pos_x + width / 2)
            label_pos_y = int(pos_y + height / 2)
            pr.draw_text(
                label,
                label_pos_x - pr.measure_text(label, 10) // 2,
                label_pos_y - 5,
                10,
                COLOR_PORTAL_LABEL,
            )

    def draw_game_objects(self):
        # Draw player
        player_x = self.game_state.player_pos_interpolated.x * CELL_SIZE
        player_y = self.game_state.player_pos_interpolated.y * CELL_SIZE
        player_w = self.game_state.player_dims.x * CELL_SIZE
        player_h = self.game_state.player_dims.y * CELL_SIZE
        pr.draw_rectangle(
            int(player_x), int(player_y), int(player_w), int(player_h), COLOR_PLAYER
        )

        # Draw other players
        for player_id, player_data in self.game_state.other_players.items():
            other_x = player_data["pos"].x * CELL_SIZE
            other_y = player_data["pos"].y * CELL_SIZE
            other_w = player_data["dims"].x * CELL_SIZE
            other_h = player_data["dims"].y * CELL_SIZE
            pr.draw_rectangle(
                int(other_x),
                int(other_y),
                int(other_w),
                int(other_h),
                COLOR_OTHER_PLAYER,
            )

        # Draw path
        if self.game_state.path:
            for i in range(len(self.game_state.path) - 1):
                start = self.game_state.path[i]
                end = self.game_state.path[i + 1]
                pr.draw_line(
                    int(start.x * CELL_SIZE + CELL_SIZE / 2),
                    int(start.y * CELL_SIZE + CELL_SIZE / 2),
                    int(end.x * CELL_SIZE + CELL_SIZE / 2),
                    int(end.y * CELL_SIZE + CELL_SIZE / 2),
                    COLOR_PATH,
                )
            # Draw target
            target_x = self.game_state.path[-1].x * CELL_SIZE
            target_y = self.game_state.path[-1].y * CELL_SIZE
            pr.draw_rectangle_lines(
                int(target_x),
                int(target_y),
                int(self.game_state.player_dims.x * CELL_SIZE),
                int(self.game_state.player_dims.y * CELL_SIZE),
                COLOR_TARGET,
            )

    def draw_debug_info(self, fps, download_kbps, upload_kbps):
        pr.draw_text(f"FPS: {fps}", 10, 10, 20, COLOR_DEBUG_TEXT)
        pr.draw_text(
            f"Download: {download_kbps:.2f} KB/s", 10, 30, 20, COLOR_DEBUG_TEXT
        )
        pr.draw_text(f"Upload: {upload_kbps:.2f} KB/s", 10, 50, 20, COLOR_DEBUG_TEXT)
        pr.draw_text(
            f"Map ID: {self.game_state.player_map_id}", 10, 70, 20, COLOR_DEBUG_TEXT
        )
        pr.draw_text(
            f"Player Mode: {self.game_state.player_mode.name}",
            10,
            90,
            20,
            COLOR_DEBUG_TEXT,
        )
        pr.draw_text(
            f"Player Direction: {self.game_state.player_direction.name}",
            10,
            110,
            20,
            COLOR_DEBUG_TEXT,
        )
        pr.draw_text(
            f"Player Dims: {self.game_state.player_dims.x}x{self.game_state.player_dims.y}",
            10,
            130,
            20,
            COLOR_DEBUG_TEXT,
        )

        if time.time() - self.game_state.error_display_time < 5.0:
            pr.draw_text(
                self.game_state.last_error_message,
                10,
                SCREEN_HEIGHT - 30,
                20,
                COLOR_ERROR_TEXT,
            )

    def run(self):
        pr.set_config_flags(pr.ConfigFlags.FLAG_WINDOW_RESIZABLE)
        pr.init_window(SCREEN_WIDTH, SCREEN_HEIGHT, "Networked Game Client")
        pr.set_target_fps(FPS)
        self.connect()

        last_download_bytes = 0
        last_upload_bytes = 0
        last_calc_time = time.time()

        while not pr.window_should_close() and self.is_running:
            # Calculate transfer speed
            current_time = time.time()
            delta_time = current_time - last_calc_time
            if delta_time >= 1.0:
                self.download_kbps = (
                    (self.game_state.download_size_bytes - last_download_bytes) / 1024
                ) / delta_time
                self.upload_kbps = (
                    (self.game_state.upload_size_bytes - last_upload_bytes) / 1024
                ) / delta_time
                last_download_bytes = self.game_state.download_size_bytes
                last_upload_bytes = self.game_state.upload_size_bytes
                last_calc_time = current_time

            # --- Interpolation Logic ---
            with self.mutex:
                time_since_update = (
                    time.time() - self.game_state.last_update_time
                ) * 1000
                # Use a linear interpolation factor
                t = min(1.0, time_since_update / INTERPOLATION_TIME_MS)

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

            # Update camera
            self.update_camera()

            # Input handling
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
                self.draw_obstacles()
                self.draw_portals()
                self.draw_game_objects()
                # Draw AOI if DEV_GUI is true
                if DEV_GUI:
                    pr.draw_circle_sector(
                        pr.Vector2(
                            (
                                self.game_state.player_pos_interpolated.x
                                + self.game_state.player_dims.x / 2
                            )
                            * CELL_SIZE,
                            (
                                self.game_state.player_pos_interpolated.y
                                + self.game_state.player_dims.y / 2
                            )
                            * CELL_SIZE,
                        ),
                        self.game_state.aoi_radius * CELL_SIZE,
                        0,
                        360,
                        32,
                        COLOR_AOI,
                    )
            pr.end_mode_2d()
            with self.mutex:
                self.draw_debug_info(pr.get_fps(), self.download_kbps, self.upload_kbps)
            pr.end_drawing()

        print("Closing WebSocket...")
        if self.ws:
            self.ws.close()
        pr.close_window()


if __name__ == "__main__":
    client = NetworkClient()
    client.run()
