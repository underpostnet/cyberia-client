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

# Colors
COLOR_BACKGROUND = pr.Color(30, 30, 30, 255)
COLOR_OBSTACLE = pr.Color(100, 100, 100, 255)
COLOR_PLAYER = pr.Color(0, 200, 255, 255)
COLOR_OTHER_PLAYER = pr.Color(255, 100, 0, 255)
COLOR_PATH = pr.Color(0, 255, 0, 100)
COLOR_TARGET = pr.Color(255, 255, 0, 255)


# --- Object-Oriented Refactoring ---
# New classes to encapsulate object state and rendering
class GameObject:
    """
    Base class for any drawable object in the game.
    Manages position, dimensions, and smoothed screen position.
    """

    def __init__(self, x, y, width, height, color):
        self.server_pos = pr.Vector2(x, y)
        self.smooth_pos = pr.Vector2(
            x, y
        )  # Smooth position is now in world coordinates
        self.width = width
        self.height = height
        self.color = color

    def update_smooth_position(self, delta_time):
        """
        Updates the smoothed screen position towards the server position.
        """
        # Simple linear interpolation for smoothing
        smoothing_factor = 15.0 * delta_time  # Adjust this for faster/slower smoothing

        if (
            pr.vector2_length(pr.vector2_subtract(self.server_pos, self.smooth_pos))
            > 0.01
        ):
            self.smooth_pos = pr.vector2_lerp(
                self.smooth_pos, self.server_pos, smoothing_factor
            )
        else:
            self.smooth_pos = self.server_pos

    def draw(self, cell_size, zoom_level):
        """
        Draws the object on the screen.
        """
        pos_x_px = self.smooth_pos.x * cell_size
        pos_y_px = self.smooth_pos.y * cell_size

        half_w_px = (self.width * cell_size) / 2
        half_h_px = (self.height * cell_size) / 2

        pr.draw_rectangle(
            int(pos_x_px - half_w_px),
            int(pos_y_px - half_h_px),
            int(self.width * cell_size),
            int(self.height * cell_size),
            self.color,
        )


class Player(GameObject):
    """
    Represents the client's own player character.
    """

    def __init__(self, x, y, width, height):
        super().__init__(x, y, width, height, COLOR_PLAYER)
        self.path = []
        self.target_pos = None


class OtherPlayer(GameObject):
    """
    Represents other players visible in the Area of Interest.
    """

    def __init__(self, player_id, x, y, width, height):
        super().__init__(x, y, width, height, COLOR_OTHER_PLAYER)
        self.player_id = player_id


class GameState:
    """
    Holds the entire state of the game on the client side.
    """

    def __init__(self):
        self.player_id = ""
        self.player = None
        self.visible_players = {}  # Dictionary of OtherPlayer objects
        self.visible_obstacles = {}
        self.camera = pr.Camera2D()
        self.camera.zoom = 1.0
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
        self.object_width = DEFAULT_OBJECT_WIDTH
        self.object_height = DEFAULT_OBJECT_HEIGHT
        self.cell_size = CELL_SIZE
        self.last_touch_state = False

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
                    self.object_width = payload["objectWidth"]
                    self.object_height = payload["objectHeight"]
                    self.cell_size = SCREEN_WIDTH / self.grid_w

                    print(
                        f"Received initial data: grid {self.grid_w}x{self.grid_h}, object size {self.object_width}x{self.object_height}"
                    )

                elif data["type"] == "aoi_update":
                    payload = data["payload"]
                    player_id = payload["playerID"]
                    player_pos = pr.Vector2(
                        payload["playerPos"]["X"], payload["playerPos"]["Y"]
                    )

                    player_dims = (
                        payload["playerDimensions"]["Width"],
                        payload["playerDimensions"]["Height"],
                    )

                    if self.game_state.player is None:
                        # Initialize player object on first update
                        self.game_state.player = Player(
                            player_pos.x, player_pos.y, player_dims[0], player_dims[1]
                        )
                        self.game_state.player_id = player_id

                    self.game_state.player.server_pos = player_pos

                    path_data = payload.get("path")
                    if path_data is not None:
                        self.game_state.player.path = [
                            (p["X"], p["Y"]) for p in path_data
                        ]
                    else:
                        self.game_state.player.path = []

                    target_pos_data = payload.get("targetPos")
                    if target_pos_data is not None:
                        self.game_state.player.target_pos = (
                            target_pos_data["X"],
                            target_pos_data["Y"],
                        )
                    else:
                        self.game_state.player.target_pos = None

                    new_visible_players = {}
                    for k, v in payload["visiblePlayers"].items():
                        if k in self.game_state.visible_players:
                            other_player = self.game_state.visible_players[k]
                            other_player.server_pos = pr.Vector2(v["X"], v["Y"])
                            new_visible_players[k] = other_player
                        else:
                            new_visible_players[k] = OtherPlayer(
                                k, v["X"], v["Y"], v["Width"], v["Height"]
                            )
                    self.game_state.visible_players = new_visible_players

                    new_visible_obstacles = {}
                    for k, v in payload["visibleGridObjects"].items():
                        new_visible_obstacles[k] = GameObject(
                            v["X"], v["Y"], v["Width"], v["Height"], COLOR_OBSTACLE
                        )
                    self.game_state.visible_obstacles = new_visible_obstacles

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

    def update_player_screen_pos(self, delta_time):
        """
        Abstracted function to update player and other players' positions.
        """
        if self.game_state.player:
            self.game_state.player.update_smooth_position(delta_time)

        with self.mutex:
            for player_id, other_player in self.game_state.visible_players.items():
                other_player.update_smooth_position(delta_time)

    def update_camera(self):
        """
        Updates the camera position to follow the player and handles zoom.
        """
        if self.game_state.player:
            player_smooth_pos_px = pr.Vector2(
                self.game_state.player.smooth_pos.x * self.cell_size,
                self.game_state.player.smooth_pos.y * self.cell_size,
            )

            # Camera target should be the player's position in world coordinates (pixels)
            self.game_state.camera.target = player_smooth_pos_px
            # Camera offset is half the screen size to center the player
            self.game_state.camera.offset = pr.Vector2(
                SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2
            )

            # Handle zoom with mouse wheel
            zoom_factor = pr.get_mouse_wheel_move()
            if zoom_factor != 0:
                self.game_state.camera.zoom += zoom_factor * 0.1
                if self.game_state.camera.zoom < 0.1:
                    self.game_state.camera.zoom = 0.1

    def get_grid_pos_from_input(self):
        """
        Gets the grid position from mouse input.
        Returns a tuple (x, y) or None.
        """
        if pr.is_mouse_button_pressed(pr.MOUSE_LEFT_BUTTON):
            screen_pos = pr.get_mouse_position()
            world_pos = pr.get_screen_to_world_2d(screen_pos, self.game_state.camera)
            grid_x = int(world_pos.x / self.cell_size)
            grid_y = int(world_pos.y / self.cell_size)
            return (grid_x, grid_y)

        return None

    def draw_grid(self):
        """
        Draws the background grid lines, scaled with zoom.
        """
        for i in range(self.grid_w + 1):
            pr.draw_line(
                int(i * self.cell_size),
                int(0),
                int(i * self.cell_size),
                int(self.grid_h * self.cell_size),
                pr.fade(pr.GRAY, 0.5),
            )
        for i in range(self.grid_h + 1):
            pr.draw_line(
                int(0),
                int(i * self.cell_size),
                int(self.grid_w * self.cell_size),
                int(i * self.cell_size),
                pr.fade(pr.GRAY, 0.5),
            )

    def draw_game_objects(self):
        """
        Draws all objects based on the game state received from the server.
        """
        with self.mutex:
            # Draw visible obstacles with a slight border
            for _, obs in self.game_state.visible_obstacles.items():
                # Obstacles are rendered as full grid cells
                obs_pos_px = pr.Vector2(
                    obs.smooth_pos.x * self.cell_size, obs.smooth_pos.y * self.cell_size
                )

                # Draw the filled rectangle for the obstacle
                pr.draw_rectangle(
                    int(obs_pos_px.x),
                    int(obs_pos_px.y),
                    int(self.cell_size),
                    int(self.cell_size),
                    obs.color,
                )

                # Draw a distinct border around the obstacle cell
                pr.draw_rectangle_lines(
                    int(obs_pos_px.x),
                    int(obs_pos_px.y),
                    int(self.cell_size),
                    int(self.cell_size),
                    pr.WHITE,
                )

            # Draw the path
            if self.game_state.player and self.game_state.player.path:
                # Start the path from the player's current position
                prev_point_px = pr.Vector2(
                    self.game_state.player.smooth_pos.x * self.cell_size,
                    self.game_state.player.smooth_pos.y * self.cell_size,
                )

                for point in self.game_state.player.path:
                    # Calculate the center of the next grid cell
                    current_point_center_px = pr.Vector2(
                        (point[0] + 0.5) * self.cell_size,
                        (point[1] + 0.5) * self.cell_size,
                    )

                    pr.draw_line_ex(
                        prev_point_px,
                        current_point_center_px,
                        self.cell_size / 4,
                        pr.fade(pr.LIME, 0.5),
                    )
                    prev_point_px = current_point_center_px

            # Draw the target position
            if self.game_state.player and self.game_state.player.target_pos:
                target_point = self.game_state.player.target_pos
                target_pos_center_px = pr.Vector2(
                    (target_point[0] + 0.5) * self.cell_size,
                    (target_point[1] + 0.5) * self.cell_size,
                )
                pr.draw_circle_v(
                    target_pos_center_px, self.cell_size * 0.4, pr.fade(pr.YELLOW, 0.7)
                )
                pr.draw_circle_lines(
                    int(target_pos_center_px.x),
                    int(target_pos_center_px.y),
                    self.cell_size * 0.4,
                    pr.YELLOW,
                )

            # Draw other visible players
            for player_id, player_obj in self.game_state.visible_players.items():
                player_obj.draw(self.cell_size, self.game_state.camera.zoom)

            # Draw the client's own player last, to be on top
            if self.game_state.player:
                self.game_state.player.draw(self.cell_size, self.game_state.camera.zoom)
                pr.draw_circle(
                    int(self.game_state.player.smooth_pos.x * self.cell_size),
                    int(self.game_state.player.smooth_pos.y * self.cell_size),
                    self.cell_size * 0.2,
                    pr.WHITE,
                )

    def draw_ui(self):
        """
        Draws the UI elements that are not affected by the camera.
        """
        with self.mutex:
            # Draw stats
            pr.draw_text(f"FPS: {self.current_fps:.0f}", 10, 10, 20, pr.GREEN)
            pr.draw_text(
                f"Download: {self.download_kbps:.2f} KB/s", 10, 40, 20, pr.GREEN
            )
            pr.draw_text(f"Upload: {self.upload_kbps:.2f} KB/s", 10, 70, 20, pr.GREEN)

            # Draw feedback message if it exists
            if self.game_state.feedback_message:
                pr.draw_text(self.game_state.feedback_message, 10, 100, 20, pr.RED)

    def run(self):
        """
        Main game loop.
        """
        pr.init_window(SCREEN_WIDTH, SCREEN_HEIGHT, "A* Pathfinding Game")
        pr.set_target_fps(FPS)

        self.ws = websocket.WebSocketApp(
            WS_URL,
            on_message=self.on_message,
            on_error=self.on_error,
            on_close=self.on_close,
            on_open=self.on_open,
        )

        ws_thread = threading.Thread(target=self.ws.run_forever)
        ws_thread.daemon = True
        ws_thread.start()

        last_stats_update = time.time()

        while not pr.window_should_close():
            current_time = time.time()
            delta_time = current_time - self.last_frame_time
            self.last_frame_time = current_time

            if current_time - last_stats_update >= 1.0:
                self.current_fps = 1.0 / delta_time if delta_time > 0 else 0
                self.download_kbps = self.game_state.download_size_bytes / 1024
                self.upload_kbps = self.game_state.upload_size_bytes / 1024
                self.game_state.download_size_bytes = 0
                self.game_state.upload_size_bytes = 0
                last_stats_update = current_time

            self.update_player_screen_pos(delta_time)
            self.update_camera()

            with self.mutex:
                if self.game_state.feedback_message:
                    self.game_state.message_timer -= pr.get_frame_time()
                    if self.game_state.message_timer <= 0:
                        self.game_state.feedback_message = None

            grid_pos = self.get_grid_pos_from_input()
            if grid_pos:
                grid_x, grid_y = grid_pos
                if 0 <= grid_x < self.grid_w and 0 <= grid_y < self.grid_h:
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

            self.draw_ui()

            pr.end_drawing()

        self.ws.close()
        pr.close_window()


if __name__ == "__main__":
    client = GameClient()
    client.run()
