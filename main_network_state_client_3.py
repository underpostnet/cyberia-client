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
CELL_SIZE = SCREEN_WIDTH / GRID_SIZE_W
FPS = 60
WS_URL = "ws://localhost:8080/ws"

# Dev GUI Flag
# Set to True to enable debug rendering (grid, AOI)
DEV_GUI = True
AOI_RADIUS = 20.0  # Client-side assumption for AOI radius, in grid units

# Colors
COLOR_BACKGROUND = pr.Color(30, 30, 30, 255)
COLOR_OBSTACLE = pr.Color(100, 100, 100, 255)
COLOR_PLAYER = pr.Color(0, 200, 255, 255)
COLOR_OTHER_PLAYER = pr.Color(255, 100, 0, 255)
COLOR_PATH = pr.Color(0, 255, 0, 100)
COLOR_TARGET = pr.Color(255, 255, 0, 255)
COLOR_AOI = pr.fade(pr.BLUE, 0.2)
COLOR_TEXT = pr.Color(255, 255, 255, 255)


# GameState: This object holds all the dynamic data received from the server.
# It is designed to be thread-safe for updates from the WebSocket thread.
class GameState:
    def __init__(self):
        self.player_id = None
        self.player_state = None
        self.visible_players = {}
        self.visible_grid_objects = {}
        self.grid_w = GRID_SIZE_W
        self.grid_h = GRID_SIZE_H
        self.download_size_bytes = 0
        self.upload_size_bytes = 0
        self.feedback_message = None
        self.message_timer = 0.0
        self.camera = pr.Camera2D()
        self.camera.zoom = 1.0


# GameClient: The main class that manages the game window, rendering, and networking.
class GameClient:
    def __init__(self, ws_url):
        self.ws_url = ws_url
        self.game_state = GameState()
        self.ws = None
        self.ws_thread = None
        self.is_running = True
        self.mutex = threading.Lock()

        # Initialize kbps variables to prevent AttributeError
        self.download_kbps = 0.0
        self.upload_kbps = 0.0

    # WebSocket communication methods
    def on_message(self, ws, message):
        self.mutex.acquire()
        self.game_state.download_size_bytes += len(message)
        self.mutex.release()

        msg = json.loads(message)
        msg_type = msg.get("type")

        if msg_type == "init_data":
            payload = msg.get("payload")
            with self.mutex:
                self.game_state.grid_w = payload.get("gridW")
                self.game_state.grid_h = payload.get("gridH")
                # AOI_RADIUS = payload.get("aoiRadius") # Not used in client for now
            print("Received initial data from server.")
        elif msg_type == "aoi_update":
            payload = msg.get("payload")
            with self.mutex:
                # Update our own player's state
                self.game_state.player_state = payload.get("player")
                self.game_state.player_id = self.game_state.player_state.get("id")
                # Update other players
                self.game_state.visible_players = payload.get("visiblePlayers")
                # Update obstacles
                self.game_state.visible_grid_objects = payload.get("visibleGridObjects")
        elif msg_type == "path_not_found":
            payload = msg.get("payload")
            with self.mutex:
                self.game_state.feedback_message = payload
                self.game_state.message_timer = 2.0
            print(f"Server Feedback: {payload}")
        else:
            print(f"Received unknown message type: {msg_type}")

    def on_error(self, ws, error):
        print(f"WebSocket Error: {error}")

    def on_close(self, ws, close_status_code, close_msg):
        print(f"WebSocket connection closed with code {close_status_code}: {close_msg}")
        self.is_running = False

    def on_open(self, ws):
        print("WebSocket connection opened.")

    def websocket_thread(self):
        self.ws = websocket.WebSocketApp(
            self.ws_url,
            on_open=self.on_open,
            on_message=self.on_message,
            on_error=self.on_error,
            on_close=self.on_close,
        )
        self.ws.run_forever()

    def send_path_request(self, target_x, target_y):
        if not self.ws or not self.ws.sock or not self.ws.sock.connected:
            print("WebSocket is not connected. Cannot send path request.")
            return

        message = {
            "type": "path_request",
            "payload": {"targetX": target_x, "targetY": target_y},
        }
        json_message = json.dumps(message)
        self.ws.send(json_message)
        self.mutex.acquire()
        self.game_state.upload_size_bytes += len(json_message)
        self.mutex.release()

    # Game logic and rendering
    def update_camera(self):
        with self.mutex:
            if self.game_state.player_state:
                player_pos = self.game_state.player_state["Pos"]
                self.game_state.camera.target = pr.Vector2(
                    player_pos["X"] * self.cell_size, player_pos["Y"] * self.cell_size
                )
                self.game_state.camera.offset = pr.Vector2(
                    SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2
                )

    def draw_grid(self):
        self.cell_size = SCREEN_WIDTH / self.game_state.grid_w
        for y in range(self.game_state.grid_h):
            for x in range(self.game_state.grid_w):
                pr.draw_rectangle_lines(
                    int(x * self.cell_size),
                    int(y * self.cell_size),
                    int(self.cell_size),
                    int(self.cell_size),
                    pr.GRAY,
                )

    def draw_game_objects(self):
        self.cell_size = SCREEN_WIDTH / self.game_state.grid_w

        with self.mutex:
            # Draw obstacles
            for obs_id, obs_state in self.game_state.visible_grid_objects.items():
                x = obs_state["Pos"]["X"] * self.cell_size
                y = obs_state["Pos"]["Y"] * self.cell_size
                w = obs_state["Dims"]["Width"] * self.cell_size
                h = obs_state["Dims"]["Height"] * self.cell_size
                pr.draw_rectangle(int(x), int(y), int(w), int(h), COLOR_OBSTACLE)

            # Draw other players
            for player_id, player_state in self.game_state.visible_players.items():
                x = player_state["Pos"]["X"] * self.cell_size
                y = player_state["Pos"]["Y"] * self.cell_size
                w = player_state["Dims"]["Width"] * self.cell_size
                h = player_state["Dims"]["Height"] * self.cell_size
                pr.draw_rectangle_rec(pr.Rectangle(x, y, w, h), COLOR_OTHER_PLAYER)

            # Draw our own player
            if self.game_state.player_state:
                player_pos = self.game_state.player_state["Pos"]
                player_dims = self.game_state.player_state["Dims"]
                x = player_pos["X"] * self.cell_size
                y = player_pos["Y"] * self.cell_size
                w = player_dims["Width"] * self.cell_size
                h = player_dims["Height"] * self.cell_size
                pr.draw_rectangle_rec(pr.Rectangle(x, y, w, h), COLOR_PLAYER)

                # Draw player path
                path = self.game_state.player_state.get("path", [])
                if len(path) > 1:
                    for i in range(len(path) - 1):
                        p1 = path[i]
                        p2 = path[i + 1]
                        pr.draw_line(
                            int(p1["X"] * self.cell_size + self.cell_size / 2),
                            int(p1["Y"] * self.cell_size + self.cell_size / 2),
                            int(p2["X"] * self.cell_size + self.cell_size / 2),
                            int(p2["Y"] * self.cell_size + self.cell_size / 2),
                            COLOR_PATH,
                        )

    def draw_debug_info(self, fps, download_kbps, upload_kbps):
        if not DEV_GUI:
            return

        pr.draw_text(f"FPS: {fps:.0f}", 10, 10, 20, COLOR_TEXT)
        pr.draw_text(f"DL: {download_kbps:.2f} KB/s", 10, 40, 20, COLOR_TEXT)
        pr.draw_text(f"UL: {upload_kbps:.2f} KB/s", 10, 70, 20, COLOR_TEXT)

        with self.mutex:
            if self.game_state.feedback_message:
                pr.draw_text(
                    self.game_state.feedback_message,
                    SCREEN_WIDTH // 2 - 100,
                    SCREEN_HEIGHT // 2,
                    30,
                    pr.RED,
                )

    def get_grid_pos_from_input(self):
        # Check for mouse input first
        if pr.is_mouse_button_pressed(pr.MOUSE_BUTTON_LEFT):
            mouse_pos = pr.get_mouse_position()
            world_pos = pr.get_screen_to_world_2d(mouse_pos, self.game_state.camera)
            grid_x = int(world_pos.x / self.cell_size)
            grid_y = int(world_pos.y / self.cell_size)
            return (grid_x, grid_y)

        # Check for touch input if supported
        if hasattr(pr, "get_touch_points_count"):
            if pr.get_touch_points_count() > 0:
                touch_pos = pr.get_touch_position(0)
                world_pos = pr.get_screen_to_world_2d(touch_pos, self.game_state.camera)
                grid_x = int(world_pos.x / self.cell_size)
                grid_y = int(world_pos.y / self.cell_size)
                return (grid_x, grid_y)

        return None

    def run(self):
        pr.init_window(SCREEN_WIDTH, SCREEN_HEIGHT, "Cyberia Client")
        pr.set_target_fps(FPS)
        pr.set_exit_key(pr.KEY_NULL)

        self.ws_thread = threading.Thread(target=self.websocket_thread)
        self.ws_thread.start()

        last_stats_update = time.time()

        while not pr.window_should_close() and self.is_running:
            delta_time = pr.get_frame_time()

            # Update stats every second
            current_time = time.time()
            if current_time - last_stats_update >= 1.0:
                self.download_kbps = self.game_state.download_size_bytes / 1024
                self.upload_kbps = self.game_state.upload_size_bytes / 1024
                self.game_state.download_size_bytes = 0
                self.game_state.upload_size_bytes = 0
                last_stats_update = current_time

            # Handle input and game logic
            self.update_camera()

            with self.mutex:
                if self.game_state.feedback_message:
                    self.game_state.message_timer -= pr.get_frame_time()
                    if self.game_state.message_timer <= 0:
                        self.game_state.feedback_message = None

            grid_pos = self.get_grid_pos_from_input()
            if grid_pos:
                grid_x, grid_y = grid_pos
                if (
                    0 <= grid_x < self.game_state.grid_w
                    and 0 <= grid_y < self.game_state.grid_h
                ):
                    print(f"Input clicked at grid ({grid_x}, {grid_y})")
                    self.send_path_request(grid_x, grid_y)
                else:
                    print("Input click outside of grid bounds.")

            pr.begin_drawing()
            pr.clear_background(COLOR_BACKGROUND)

            pr.begin_mode_2d(self.game_state.camera)
            self.draw_grid()
            self.draw_game_objects()
            pr.end_mode_2d()

            # Draw UI on top of the 2D world
            pr.draw_fps(10, 10)
            self.draw_debug_info(pr.get_fps(), self.download_kbps, self.upload_kbps)
            pr.end_drawing()

        # Clean up
        self.is_running = False
        if self.ws:
            self.ws.close()
        if self.ws_thread:
            self.ws_thread.join()
        pr.close_window()


if __name__ == "__main__":
    game_client = GameClient(WS_URL)
    game_client.run()
