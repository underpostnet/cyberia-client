import pyray as pr
import websocket
import threading
import json
import time
import math
import sys

# --- Game Constants ---
# These are client-side defaults and will be updated by the server
SCREEN_WIDTH = 800
SCREEN_HEIGHT = 800
GRID_SIZE_W = 100
GRID_SIZE_H = 100
OBJECT_SIZE = 1.0

# Calculated values
CELL_SIZE = SCREEN_WIDTH / GRID_SIZE_W
FPS = 60
WS_URL = "ws://localhost:8080/ws"

# Colors
COLOR_BACKGROUND = pr.Color(30, 30, 30, 255)
COLOR_OBSTACLE = pr.Color(100, 100, 100, 255)
COLOR_PLAYER = pr.Color(0, 200, 255, 255)
COLOR_OTHER_PLAYER = pr.Color(255, 100, 0, 255)
COLOR_PATH = pr.Color(0, 255, 0, 100)
COLOR_TARGET = pr.Color(255, 255, 0, 255)


# --- Client-side State Management ---
class GameState:
    """
    Holds the entire state of the game on the client side.
    """

    def __init__(self):
        self.player_id = ""
        self.player_pos = None  # Server position (grid coordinates)
        self.player_screen_pos = None  # Screen position (float for smoothing)
        self.visible_players = {}
        self.visible_obstacles = {}
        self.path = []
        self.target_pos = None
        self.camera = pr.Camera2D()
        self.feedback_message = None
        self.message_timer = 0
        self.download_size_bytes = 0
        self.upload_size_bytes = 0


class GameClient:
    """
    Encapsulates all the game's logic, state, and rendering.
    """

    def __init__(self):
        self.game_state = GameState()
        self.ws = None
        self.mutex = threading.Lock()
        self.last_frame_time = time.time()
        self.current_fps = 0
        self.download_kbps = 0
        self.upload_kbps = 0
        self.grid_w = GRID_SIZE_W
        self.grid_h = GRID_SIZE_H
        self.object_size = OBJECT_SIZE

    def on_message(self, ws, message):
        """
        Handles incoming WebSocket messages from the server.
        """
        try:
            self.game_state.download_size_bytes += len(message)

            data = json.loads(message)
            with self.mutex:
                if data["type"] == "init_data":
                    payload = data["payload"]
                    self.grid_w = payload["gridW"]
                    self.grid_h = payload["gridH"]
                    self.object_size = payload["objectSize"]
                    print(
                        f"Received initial data: grid {self.grid_w}x{self.grid_h}, object size {self.object_size}"
                    )

                elif data["type"] == "aoi_update":
                    payload = data["payload"]
                    self.game_state.player_id = payload["PlayerID"]
                    new_pos = (payload["PlayerPos"]["X"], payload["PlayerPos"]["Y"])

                    if self.game_state.player_pos is None:
                        # Set initial player screen position
                        self.game_state.player_screen_pos = (
                            new_pos[0] * CELL_SIZE,
                            new_pos[1] * CELL_SIZE,
                        )

                    self.game_state.player_pos = new_pos
                    self.game_state.visible_players = {
                        k: (v["X"], v["Y"])
                        for k, v in payload["VisiblePlayers"].items()
                    }

                    visible_obstacles = {}
                    for k, v in payload["VisibleGridObjects"].items():
                        x_str, y_str = k.split(",")
                        visible_obstacles[(int(x_str), int(y_str))] = v
                    self.game_state.visible_obstacles = visible_obstacles

                    self.game_state.path = [(p["X"], p["Y"]) for p in payload["Path"]]
                    self.game_state.target_pos = (
                        (payload["TargetPos"]["X"], payload["TargetPos"]["Y"])
                        if payload["TargetPos"]
                        else None
                    )

                elif data["type"] == "path_not_found":
                    self.game_state.feedback_message = data["payload"]
                    self.game_state.message_timer = 3.0

        except json.JSONDecodeError as e:
            print(f"Failed to decode JSON: {e}")

    def on_error(self, ws, error):
        print(f"WebSocket error: {error}")

    def on_close(self, ws, close_status_code, close_msg):
        print(f"WebSocket closed with code {close_status_code}: {close_msg}")

    def on_open(self, ws):
        print("WebSocket connection opened.")

    def send_path_request(self, x, y):
        """
        Sends a path request message to the server.
        """
        if self.ws and self.ws.sock and self.ws.sock.connected:
            message = {"type": "path_request", "payload": {"x": x, "y": y}}
            json_message = json.dumps(message)
            self.ws.send(json_message)
            self.game_state.upload_size_bytes += sys.getsizeof(json_message)

    def draw_square(self, x, y, size, color):
        """
        Draws a square object based on the given grid coordinates.
        This function is now used for all drawable game objects.
        """
        # Ensure the object is drawn with the server-provided size, centered in the grid cell
        cell_center_x = x * CELL_SIZE + CELL_SIZE / 2
        cell_center_y = y * CELL_SIZE + CELL_SIZE / 2
        half_obj_size_px = size * CELL_SIZE / 2

        pr.draw_rectangle(
            int(cell_center_x - half_obj_size_px),
            int(cell_center_y - half_obj_size_px),
            int(size * CELL_SIZE),
            int(size * CELL_SIZE),
            color,
        )

    def draw_grid(self):
        """
        Draws the background grid lines, restricted to the game world's size.
        """
        for i in range(self.grid_w + 1):
            pr.draw_line(
                int(i * CELL_SIZE),
                0,
                int(i * CELL_SIZE),
                int(self.grid_h * CELL_SIZE),
                pr.GRAY,
            )
        for i in range(self.grid_h + 1):
            pr.draw_line(
                0,
                int(i * CELL_SIZE),
                int(self.grid_w * CELL_SIZE),
                int(i * CELL_SIZE),
                pr.GRAY,
            )

    def draw_game_objects(self):
        """
        Draws all objects based on the game state received from the server.
        """
        with self.mutex:
            # Draw visible obstacles
            for x, y in self.game_state.visible_obstacles:
                self.draw_square(x, y, self.object_size, COLOR_OBSTACLE)

            # Draw the path
            if self.game_state.path:
                for point in self.game_state.path:
                    self.draw_square(point[0], point[1], self.object_size, COLOR_PATH)

            # Draw the target position
            if self.game_state.target_pos:
                x, y = self.game_state.target_pos
                self.draw_square(x, y, self.object_size, COLOR_TARGET)

            # Draw other players
            for _, pos in self.game_state.visible_players.items():
                self.draw_square(pos[0], pos[1], self.object_size, COLOR_OTHER_PLAYER)

            # Draw the main player (on top of everything)
            if self.game_state.player_screen_pos:
                # The player is now drawn as a square
                half_obj_size_px = self.object_size * CELL_SIZE / 2
                pr.draw_rectangle(
                    int(
                        self.game_state.player_screen_pos[0]
                        + CELL_SIZE / 2
                        - half_obj_size_px
                    ),
                    int(
                        self.game_state.player_screen_pos[1]
                        + CELL_SIZE / 2
                        - half_obj_size_px
                    ),
                    int(self.object_size * CELL_SIZE),
                    int(self.object_size * CELL_SIZE),
                    COLOR_PLAYER,
                )

    def update_camera(self):
        """
        Updates the camera position to smoothly follow the player.
        """
        with self.mutex:
            if self.game_state.player_screen_pos:
                player_center = pr.Vector2(
                    self.game_state.player_screen_pos[0] + CELL_SIZE / 2,
                    self.game_state.player_screen_pos[1] + CELL_SIZE / 2,
                )
                self.game_state.camera.target = player_center
                self.game_state.camera.offset = pr.Vector2(
                    SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2
                )
                self.game_state.camera.rotation = 0.0
                self.game_state.camera.zoom = 1.0

    def update_player_screen_pos(self):
        """
        Smoothly interpolates the player's screen position towards the latest server position.
        """
        with self.mutex:
            if self.game_state.player_pos and self.game_state.player_screen_pos:
                target_x = self.game_state.player_pos[0] * CELL_SIZE
                target_y = self.game_state.player_pos[1] * CELL_SIZE

                current_x, current_y = self.game_state.player_screen_pos

                lerp_factor = 0.1
                new_x = current_x + (target_x - current_x) * lerp_factor
                new_y = current_y + (target_y - current_y) * lerp_factor

                self.game_state.player_screen_pos = (new_x, new_y)

    def draw_ui(self):
        """
        Draws the game's user interface, including performance metrics.
        """
        with self.mutex:
            if self.game_state.player_id:
                pr.draw_text(
                    f"Player ID: {self.game_state.player_id}", 10, 10, 14, pr.RAYWHITE
                )

            pr.draw_text(f"FPS: {self.current_fps:.0f}", 10, 30, 14, pr.RAYWHITE)
            pr.draw_text(
                f"Download: {self.download_kbps:.2f} KB/s", 10, 50, 14, pr.RAYWHITE
            )
            pr.draw_text(
                f"Upload: {self.upload_kbps:.2f} KB/s", 10, 70, 14, pr.RAYWHITE
            )

            num_objects = (
                len(self.game_state.visible_players)
                + len(self.game_state.visible_obstacles)
                + len(self.game_state.path)
                + (1 if self.game_state.player_pos else 0)
                + (1 if self.game_state.target_pos else 0)
            )
            pr.draw_text(f"Objects Loaded: {num_objects}", 10, 90, 14, pr.RAYWHITE)

            if self.game_state.feedback_message:
                pr.draw_text(self.game_state.feedback_message, 10, 110, 20, pr.RAYWHITE)

    def run(self):
        """
        Initializes pyray and runs the main game loop.
        """
        pr.init_window(SCREEN_WIDTH, SCREEN_HEIGHT, "MMO Prototype Client")
        pr.set_target_fps(FPS)

        self.ws = websocket.WebSocketApp(
            WS_URL,
            on_open=self.on_open,
            on_message=self.on_message,
            on_error=self.on_error,
            on_close=self.on_close,
        )
        ws_thread = threading.Thread(target=self.ws.run_forever, daemon=True)
        ws_thread.start()

        last_stats_update = time.time()

        while not pr.window_should_close():
            current_time = time.time()
            delta_time = current_time - self.last_frame_time
            self.last_frame_time = current_time
            if delta_time > 0:
                self.current_fps = 1.0 / delta_time

            if current_time - last_stats_update >= 1.0:
                self.download_kbps = self.game_state.download_size_bytes / 1024
                self.upload_kbps = self.game_state.upload_size_bytes / 1024
                self.game_state.download_size_bytes = 0
                self.game_state.upload_size_bytes = 0
                last_stats_update = current_time

            self.update_player_screen_pos()
            self.update_camera()

            with self.mutex:
                if self.game_state.feedback_message:
                    self.game_state.message_timer -= pr.get_frame_time()
                    if self.game_state.message_timer <= 0:
                        self.game_state.feedback_message = None

            if pr.is_mouse_button_pressed(pr.MOUSE_LEFT_BUTTON):
                with self.mutex:
                    mouse_screen_pos = pr.get_mouse_position()
                    world_pos = pr.get_screen_to_world_2d(
                        mouse_screen_pos, self.game_state.camera
                    )
                    grid_x = int(world_pos.x // CELL_SIZE)
                    grid_y = int(world_pos.y // CELL_SIZE)
                    print(
                        f"Mouse clicked at world ({world_pos.x}, {world_pos.y}), grid ({grid_x}, {grid_y})"
                    )
                    self.send_path_request(grid_x, grid_y)

            pr.begin_drawing()
            pr.clear_background(COLOR_BACKGROUND)

            pr.begin_mode_2d(self.game_state.camera)
            self.draw_grid()
            self.draw_game_objects()
            pr.end_mode_2d()

            self.draw_ui()

            pr.end_drawing()

        pr.close_window()


if __name__ == "__main__":
    client = GameClient()
    client.run()
