import pyray as pr
import websocket
import threading
import json
import time
import math
import sys

# --- Game Constants ---
# These are client-side defaults and will be updated by the server
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
INTERPOLATION_TIME_MS = 100

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
COLOR_AOI = pr.fade(pr.PURPLE, 0.2)  # Changed from BLUE to PURPLE
COLOR_DEBUG_TEXT = pr.Color(200, 200, 200, 255)


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
        self.other_players = {}
        self.path = []
        self.target_pos = pr.Vector2(-1, -1)
        self.aoi_rect = pr.Rectangle(0, 0, 0, 0)
        self.last_update_time = time.time()

        # Performance metrics
        self.download_size_bytes = 0
        self.upload_size_bytes = 0

        # Pyray camera
        self.camera = pr.Camera2D()
        self.camera.zoom = 2.0  # Increased zoom
        self.camera.offset = pr.Vector2(
            pr.get_screen_width() / 2, pr.get_screen_height() / 2
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
        self.player_id = None

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
                    pr.set_window_size(
                        self.game_state.grid_w * 10, self.game_state.grid_h * 10
                    )

                    # Update camera offset for the new window size
                    self.game_state.camera.offset = pr.Vector2(
                        pr.get_screen_width() / 2, pr.get_screen_height() / 2
                    )

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

                        # Update visible players
                        visible_players_data = payload.get("visiblePlayers", {})
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
                            }

                        # Update visible obstacles
                        visible_objects_data = payload.get("visibleGridObjects", {})
                        self.game_state.obstacles = {}
                        for obj_id, obj_state in visible_objects_data.items():
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
                else:
                    pass  # Ignore unknown messages to reduce log spam

            except json.JSONDecodeError as e:
                print(f"Failed to decode JSON: {e}")
            except Exception as e:
                print(f"An error occurred: {e}")

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
            pr.draw_line(
                int(x * CELL_SIZE),
                0,
                int(x * CELL_SIZE),
                int(grid_h * CELL_SIZE),
                pr.GRAY,
            )
        for y in range(grid_h):
            pr.draw_line(
                0,
                int(y * CELL_SIZE),
                int(grid_w * CELL_SIZE),
                int(y * CELL_SIZE),
                pr.GRAY,
            )

    def draw_game_objects(self):
        # Draw obstacles
        with self.mutex:
            # Interpolate player position
            t = (
                (time.time() - self.game_state.last_update_time)
                * 1000.0
                / INTERPOLATION_TIME_MS
            )
            if t > 1.0:
                t = 1.0

            # Linear interpolation
            self.game_state.player_pos_interpolated.x = (
                self.game_state.player_pos_prev.x
                + (
                    self.game_state.player_pos_server.x
                    - self.game_state.player_pos_prev.x
                )
                * t
            )
            self.game_state.player_pos_interpolated.y = (
                self.game_state.player_pos_prev.y
                + (
                    self.game_state.player_pos_server.y
                    - self.game_state.player_pos_prev.y
                )
                * t
            )

            # Update camera position to follow the interpolated player position
            self.game_state.camera.target = pr.Vector2(
                self.game_state.player_pos_interpolated.x * CELL_SIZE
                + (self.game_state.player_dims.x * CELL_SIZE) / 2,
                self.game_state.player_pos_interpolated.y * CELL_SIZE
                + (self.game_state.player_dims.y * CELL_SIZE) / 2,
            )

            for _, obj in self.game_state.obstacles.items():
                pos = obj["pos"]
                dims = obj["dims"]
                pr.draw_rectangle_v(
                    pr.Vector2(pos.x * CELL_SIZE, pos.y * CELL_SIZE),
                    pr.Vector2(dims.x * CELL_SIZE, dims.y * CELL_SIZE),
                    COLOR_OBSTACLE,
                )

            # Draw other players
            for _, player in self.game_state.other_players.items():
                pos = player["pos"]
                dims = player["dims"]
                pr.draw_rectangle_v(
                    pr.Vector2(pos.x * CELL_SIZE, pos.y * CELL_SIZE),
                    pr.Vector2(dims.x * CELL_SIZE, dims.y * CELL_SIZE),
                    COLOR_OTHER_PLAYER,
                )

            # Draw own player using the interpolated position
            player_pos = self.game_state.player_pos_interpolated
            player_dims = self.game_state.player_dims
            pr.draw_rectangle_v(
                pr.Vector2(player_pos.x * CELL_SIZE, player_pos.y * CELL_SIZE),
                pr.Vector2(player_dims.x * CELL_SIZE, player_dims.y * CELL_SIZE),
                COLOR_PLAYER,
            )

            # Draw path and target
            if DEV_GUI and len(self.game_state.path) > 0:
                pr.draw_rectangle_v(
                    pr.Vector2(
                        self.game_state.target_pos.x * CELL_SIZE,
                        self.game_state.target_pos.y * CELL_SIZE,
                    ),
                    pr.Vector2(CELL_SIZE, CELL_SIZE),
                    COLOR_TARGET,
                )
                for i in range(len(self.game_state.path) - 1):
                    p1 = self.game_state.path[i]
                    p2 = self.game_state.path[i + 1]
                    pr.draw_line_ex(
                        pr.Vector2(
                            p1.x * CELL_SIZE + CELL_SIZE / 2,
                            p1.y * CELL_SIZE + CELL_SIZE / 2,
                        ),
                        pr.Vector2(
                            p2.x * CELL_SIZE + CELL_SIZE / 2,
                            p2.y * CELL_SIZE + CELL_SIZE / 2,
                        ),
                        3,
                        COLOR_PATH,
                    )

    def draw_aoi(self):
        if not DEV_GUI:
            return

        with self.mutex:
            player_pos = self.game_state.player_pos_interpolated
            aoi_radius = self.game_state.aoi_radius

            # The player's position is the top-left corner of the object.
            # We want to draw the AOI around the center of the player object.
            player_center_x = (
                player_pos.x * CELL_SIZE
                + (self.game_state.player_dims.x * CELL_SIZE) / 2
            )
            player_center_y = (
                player_pos.y * CELL_SIZE
                + (self.game_state.player_dims.y * CELL_SIZE) / 2
            )

            # The AOI is a square with side length 2 * aoi_radius
            aoi_side = aoi_radius * 2 * CELL_SIZE
            aoi_rect = pr.Rectangle(
                player_center_x - aoi_side / 2,
                player_center_y - aoi_side / 2,
                aoi_side,
                aoi_side,
            )
            pr.draw_rectangle_rec(aoi_rect, COLOR_AOI)

    def draw_debug_info(self, fps, download_kbps, upload_kbps):
        pr.draw_text(f"FPS: {fps}", 10, 10, 20, COLOR_DEBUG_TEXT)
        pr.draw_text(
            f"Download: {download_kbps:.2f} KB/s", 10, 40, 20, COLOR_DEBUG_TEXT
        )
        pr.draw_text(f"Upload: {upload_kbps:.2f} KB/s", 10, 70, 20, COLOR_DEBUG_TEXT)
        if self.player_id:
            pr.draw_text(
                f"Player ID: {self.player_id[:8]}...", 10, 100, 20, COLOR_DEBUG_TEXT
            )

    def calculate_bandwidth(self):
        while self.is_running:
            time.sleep(1)
            with self.mutex:
                self.download_kbps = self.game_state.download_size_bytes / 1024
                self.upload_kbps = self.game_state.upload_size_bytes / 1024
                self.game_state.download_size_bytes = 0
                self.game_state.upload_size_bytes = 0

    def run(self):
        # Set up a new thread for calculating bandwidth
        bandwidth_thread = threading.Thread(target=self.calculate_bandwidth)
        bandwidth_thread.start()

        pr.init_window(SCREEN_WIDTH, SCREEN_HEIGHT, "Networked Game Client")
        pr.set_target_fps(FPS)

        # Connect to the WebSocket in a separate thread
        websocket.enableTrace(False)  # Disabling this to reduce log spam
        self.ws = websocket.WebSocketApp(
            WS_URL,
            on_open=self.on_open,
            on_message=self.on_message,
            on_error=self.on_error,
            on_close=self.on_close,
        )

        # --- Graceful shutdown procedure ---
        self.ws_thread = threading.Thread(target=self.ws.run_forever, daemon=True)
        self.ws_thread.start()

        while not pr.window_should_close():
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
        bandwidth_thread.join()

        pr.close_window()
        sys.exit()


if __name__ == "__main__":
    client = NetworkClient()
    client.run()
