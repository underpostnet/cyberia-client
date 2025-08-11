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
COLOR_PATH = pr.fade(pr.GREEN, 0.5)
COLOR_TARGET = pr.Color(255, 255, 0, 255)
COLOR_AOI = pr.fade(pr.PURPLE, 0.2)  # Changed from BLUE to PURPLE
COLOR_TEXT = pr.Color(255, 255, 255, 255)
COLOR_FEEDBACK = pr.Color(255, 0, 0, 255)


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
        # --- Interpolation-related state variables ---
        # Current position used for rendering
        self.player_render_pos = {"X": 0.0, "Y": 0.0}
        # Last known position from server
        self.player_last_server_pos = {"X": 0.0, "Y": 0.0}
        # Newest position from server
        self.player_target_server_pos = {"X": 0.0, "Y": 0.0}
        # Time when the last server update was received
        self.last_update_time = time.time()


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
                # Store the last known server position and the new one for interpolation
                # This is the key change to prevent jerky movement
                if self.game_state.player_state:
                    self.game_state.player_last_server_pos = (
                        self.game_state.player_state["Pos"]
                    )
                else:
                    # On the very first update, set both last and target to the same value
                    self.game_state.player_last_server_pos = payload.get("player")[
                        "Pos"
                    ]

                self.game_state.player_state = payload.get("player")
                self.game_state.player_id = self.game_state.player_state.get("id")
                self.game_state.player_target_server_pos = self.game_state.player_state[
                    "Pos"
                ]
                self.game_state.last_update_time = time.time()

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

        with self.mutex:
            # Check if the new target is the same as the current target
            player_target = self.game_state.player_state.get("targetPos")
            if (
                player_target
                and player_target["X"] == target_x
                and player_target["Y"] == target_y
            ):
                print("Target is the same, not sending new path request.")
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
    def update_player_interpolation(self):
        # This function interpolates the player's position between server updates
        with self.mutex:
            if not self.game_state.player_state:
                return

            # How much time has passed since the last server update?
            time_since_update = time.time() - self.game_state.last_update_time
            # Calculate interpolation factor. We want to complete the interpolation
            # within a single server tick (1/60 seconds).
            # This is a key part of the fix.
            interpolation_factor = min(1.0, time_since_update / (1.0 / 60.0))

            # Interpolate between the last known server position and the new one
            x_last = self.game_state.player_last_server_pos["X"]
            y_last = self.game_state.player_last_server_pos["Y"]
            x_target = self.game_state.player_target_server_pos["X"]
            y_target = self.game_state.player_target_server_pos["Y"]

            # Linear interpolation
            self.game_state.player_render_pos["X"] = (
                x_last + (x_target - x_last) * interpolation_factor
            )
            self.game_state.player_render_pos["Y"] = (
                y_last + (y_target - y_last) * interpolation_factor
            )

    def update_camera(self):
        with self.mutex:
            if self.game_state.player_state:
                # Use the interpolated position for the camera
                player_pos = self.game_state.player_render_pos
                self.game_state.camera.target = pr.Vector2(
                    player_pos["X"] * self.cell_size, player_pos["Y"] * self.cell_size
                )
                self.game_state.camera.offset = pr.Vector2(
                    SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2
                )

    def draw_aoi(self):
        # Draw the AOI circle for the player if DEV_GUI is active
        if DEV_GUI and self.game_state.player_state:
            player_pos = self.game_state.player_render_pos
            pr.draw_circle_v(
                pr.Vector2(
                    player_pos["X"] * self.cell_size, player_pos["Y"] * self.cell_size
                ),
                AOI_RADIUS * self.cell_size,
                COLOR_AOI,
            )

    def draw_grid(self):
        # Calculate cell_size inside the drawing function to ensure it's always up-to-date
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
        # Calculate cell_size inside the drawing function to ensure it's always up-to-date
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

            # Draw our own player using the interpolated position
            if self.game_state.player_state:
                player_pos = self.game_state.player_render_pos
                player_dims = self.game_state.player_state["Dims"]
                x = player_pos["X"] * self.cell_size
                y = player_pos["Y"] * self.cell_size
                w = player_dims["Width"] * self.cell_size
                h = player_dims["Height"] * self.cell_size
                pr.draw_rectangle_rec(pr.Rectangle(x, y, w, h), COLOR_PLAYER)

            # Draw player path and target
            if DEV_GUI and self.game_state.player_state:
                player_path = self.game_state.player_state.get("path", [])
                if player_path:
                    # Draw path lines
                    start_pos = self.game_state.player_render_pos
                    for next_point in player_path:
                        end_pos = pr.Vector2(
                            next_point["X"] * self.cell_size,
                            next_point["Y"] * self.cell_size,
                        )
                        pr.draw_line(
                            int(start_pos["X"] * self.cell_size),
                            int(start_pos["Y"] * self.cell_size),
                            int(end_pos.x),
                            int(end_pos.y),
                            COLOR_PATH,
                        )
                        start_pos = next_point

                    # Draw path points
                    for path_point in player_path:
                        pr.draw_circle(
                            int(path_point["X"] * self.cell_size),
                            int(path_point["Y"] * self.cell_size),
                            5,
                            pr.GREEN,
                        )

                # Draw the target point
                target_pos = self.game_state.player_state.get("targetPos")
                if target_pos:
                    pr.draw_circle_v(
                        pr.Vector2(
                            target_pos["X"] * self.cell_size,
                            target_pos["Y"] * self.cell_size,
                        ),
                        8,
                        COLOR_TARGET,
                    )

    def get_grid_pos_from_input(self):
        mouse_pos = pr.get_mouse_position()
        camera = self.game_state.camera

        # Get the mouse position in the 2D world space
        world_mouse_pos = pr.get_screen_to_world_2d(mouse_pos, camera)

        # Determine the cell_size dynamically
        cell_size = (
            SCREEN_WIDTH / self.game_state.grid_w if self.game_state.grid_w > 0 else 1
        )

        # Convert world position to grid coordinates
        grid_x = int(world_mouse_pos.x / cell_size)
        grid_y = int(world_mouse_pos.y / cell_size)

        # Ensure click is on the left mouse button
        if pr.is_mouse_button_pressed(pr.MOUSE_LEFT_BUTTON):
            return grid_x, grid_y

        return None

    def draw_debug_info(self, fps, download_kbps, upload_kbps):
        if not DEV_GUI:
            return

        with self.mutex:
            player_pos = self.game_state.player_render_pos
            pr.draw_text_ex(
                pr.get_font_default(),
                f"FPS: {int(fps)}",
                pr.Vector2(10, 10),
                20,
                2,
                COLOR_TEXT,
            )
            pr.draw_text_ex(
                pr.get_font_default(),
                f"DL: {download_kbps:.2f} KB/s",
                pr.Vector2(10, 30),
                20,
                2,
                COLOR_TEXT,
            )
            pr.draw_text_ex(
                pr.get_font_default(),
                f"UL: {upload_kbps:.2f} KB/s",
                pr.Vector2(10, 50),
                20,
                2,
                COLOR_TEXT,
            )
            pr.draw_text_ex(
                pr.get_font_default(),
                f"Player Pos: ({player_pos['X']:.2f}, {player_pos['Y']:.2f})",
                pr.Vector2(10, 70),
                20,
                2,
                COLOR_TEXT,
            )

            if self.game_state.feedback_message:
                pr.draw_text_ex(
                    pr.get_font_default(),
                    self.game_state.feedback_message,
                    pr.Vector2(
                        SCREEN_WIDTH / 2
                        - pr.measure_text(self.game_state.feedback_message, 40) / 2,
                        SCREEN_HEIGHT / 2,
                    ),
                    40,
                    2,
                    COLOR_FEEDBACK,
                )

    def run(self):
        pr.set_config_flags(pr.ConfigFlags.FLAG_WINDOW_RESIZABLE)
        pr.init_window(SCREEN_WIDTH, SCREEN_HEIGHT, "Game Client")
        pr.set_target_fps(FPS)
        pr.set_exit_key(pr.KEY_NULL)

        # Start WebSocket connection in a separate thread
        self.ws_thread = threading.Thread(target=self.websocket_thread)
        self.ws_thread.start()

        # Create a thread to calculate bandwidth
        bandwidth_thread = threading.Thread(target=self.calculate_bandwidth)
        bandwidth_thread.daemon = True  # Daemonize thread so it exits with the main app
        bandwidth_thread.start()

        last_time = time.time()

        while not pr.window_should_close() and self.is_running:
            # Main game loop
            current_time = time.time()
            frame_time = current_time - last_time
            last_time = current_time

            # Update game state based on input and game logic
            self.update_player_interpolation()  # New interpolation call
            self.update_camera()

            with self.mutex:
                if self.game_state.feedback_message:
                    self.game_state.message_timer -= frame_time
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
            self.draw_aoi()  # New call to draw the AOI
            pr.end_mode_2d()

            # Draw UI on top of the 2D world
            self.draw_debug_info(pr.get_fps(), self.download_kbps, self.upload_kbps)
            pr.end_drawing()

        # Clean up
        self.is_running = False
        if self.ws:
            self.ws.close()
        if self.ws_thread:
            self.ws_thread.join()
        pr.close_window()
        sys.exit()

    def calculate_bandwidth(self):
        while self.is_running:
            time.sleep(1)
            with self.mutex:
                self.download_kbps = self.game_state.download_size_bytes / 1024
                self.upload_kbps = self.game_state.upload_size_bytes / 1024
                self.game_state.download_size_bytes = 0
                self.game_state.upload_size_bytes = 0


if __name__ == "__main__":
    client = GameClient(WS_URL)
    client.run()
