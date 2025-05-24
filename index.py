import argparse
import logging
import random
import time
import math
import threading
import json

# Import the plain WebSocket client library
import websocket  # pip install websocket-client

# Import the external astar library
from astar import astar

from raylibpy import (
    Color,
    Vector2,
    Camera2D,
    RAYWHITE,
    DARKGRAY,
    LIGHTGRAY,
    BLACK,
    BLUE,
    RED,
    GREEN,
    MOUSE_BUTTON_LEFT,
    init_window,
    set_target_fps,
    begin_drawing,
    clear_background,
    begin_mode2d,
    draw_line,
    draw_rectangle,
    draw_circle,
    draw_text,
    end_mode2d,
    get_frame_time,
    get_mouse_position,
    get_screen_to_world2d,
    close_window,
    window_should_close,
    is_mouse_button_pressed,
    end_drawing,
)

# --- Logging Configuration ---
logging.basicConfig(
    level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s"
)

# --- Game Constants ---
SERVER_HOST = "127.0.0.1"
SERVER_PORT = 5000
WEBSOCKET_PATH = "/ws"  # The endpoint for WebSocket connections on the Go server

# Raylib window dimensions (camera viewport)
SCREEN_WIDTH = 800
SCREEN_HEIGHT = 800

# World dimensions (the grid/canvas)
WORLD_WIDTH = 1600
WORLD_HEIGHT = 1600

# Size of game objects (players, obstacles)
OBJECT_SIZE = 50

# A* maze simplification: each cell in the 32x32 maze represents a block of this size in the world
MAZE_CELL_WORLD_SIZE = WORLD_WIDTH // 32  # 1600 / 32 = 50

# Camera smoothing factor (0.0 to 1.0, higher means faster smoothing)
CAMERA_SMOOTHNESS = 0.05

# --- Game Entities ---


class GameObject:
    """
    Represents a generic object in the game world.
    """

    def __init__(
        self,
        obj_id: str,
        x: float,
        y: float,
        color: Color,
        is_obstacle: bool = False,
        speed: float = 200.0,
    ):
        self.obj_id = obj_id
        self.x = x
        self.y = y
        self.color = color
        self.is_obstacle = is_obstacle
        self.speed = speed  # Pixels per second
        self.path: list[dict[str, float]] = (
            []
        )  # List of {'X': float, 'Y': float} world coordinates to follow
        self.path_index = 0

    def to_dict(self) -> dict:
        """Converts object state to a dictionary for serialization."""
        return {
            "obj_id": self.obj_id,
            "x": self.x,
            "y": self.y,
            "color": {
                "R": self.color.r,
                "G": self.color.g,
                "B": self.color.b,
                "A": self.color.a,
            },
            "is_obstacle": self.is_obstacle,
            "speed": self.speed,
            "path": self.path,
            "path_index": self.path_index,
        }

    @classmethod
    def from_dict(cls, data: dict) -> "GameObject":
        """Creates a GameObject instance from a dictionary."""
        color_data = data["color"]
        color = Color(
            color_data["R"], color_data["G"], color_data["B"], color_data["A"]
        )
        obj = cls(
            data["obj_id"],
            data["x"],
            data["y"],
            color,
            data["is_obstacle"],
            data.get("speed", 200.0),  # Default speed if not provided
        )
        # Ensure path is always a list, even if Go sends null for empty slice
        obj.path = data.get("path") if data.get("path") is not None else []
        obj.path_index = data.get("path_index", 0)
        return obj

    def update_position(self, delta_time: float):
        """
        Updates the object's position, moving it along its path.
        Movement is smoothed based on delta_time and object speed.
        """
        if not self.path or self.path_index >= len(self.path):
            self.path = []
            self.path_index = 0
            return

        target_point = self.path[self.path_index]
        target_world_x = target_point["X"]
        target_world_y = target_point["Y"]

        # Calculate distance to target
        dx = target_world_x - self.x
        dy = target_world_y - self.y
        distance = math.sqrt(dx * dx + dy * dy)

        move_distance = (
            self.speed * delta_time
        )  # Distance the object can move in this frame

        if distance < move_distance:
            # If the distance is less than what it can move, it means it has reached or overshot the point.
            # Move exactly to the point and advance to the next one in the path.
            self.x = target_world_x
            self.y = target_world_y
            self.path_index += 1
        else:
            # Move towards the target point
            direction_x = dx / distance  # X component of the normalized direction
            direction_y = dy / distance  # Y component of the normalized direction
            self.x += direction_x * move_distance
            self.y += direction_y * move_distance


class GameState:
    """
    Manages the overall state of the game world, including objects and the grid.
    Handles conceptual grid for object placement and A* pathfinding (client-side simplified maze).
    """

    def __init__(self, world_width: int, world_height: int, object_size: int):
        self.world_width = world_width
        self.world_height = world_height
        self.object_size = object_size
        self.objects: dict[str, GameObject] = {}  # Dictionary: obj_id -> GameObject

        # Initialize the 1600x1600 grid (stores references to objects or None)
        # This grid is conceptual for object placement, not for A* directly.
        self.grid_cells_x = world_width // object_size
        self.grid_cells_y = world_height // object_size
        self.grid: list[list[GameObject | None]] = [
            [None for _ in range(self.grid_cells_x)] for _ in range(self.grid_cells_y)
        ]

        # Initialize the 32x32 simplified maze for A*
        self.maze_cells_x = world_width // MAZE_CELL_WORLD_SIZE
        self.maze_cells_y = world_height // MAZE_CELL_WORLD_SIZE
        self.simplified_maze: list[list[int]] = [
            [0 for _ in range(self.maze_cells_x)] for _ in range(self.maze_cells_y)
        ]
        self._build_simplified_maze()  # Initial build

        self.lock = threading.Lock()  # To protect game state from concurrent access

    def _world_to_grid_coords(self, world_x: float, world_y: float) -> tuple[int, int]:
        """Converts world coordinates to grid cell indices."""
        grid_x = world_x // self.object_size
        grid_y = world_y // self.object_size
        return int(grid_x), int(grid_y)

    def _grid_to_world_coords(self, grid_x: int, grid_y: int) -> tuple[float, float]:
        """Converts grid cell indices to world coordinates (top-left of cell)."""
        world_x = float(grid_x * self.object_size)
        world_y = float(grid_y * self.object_size)
        return world_x, world_y

    def world_to_maze_coords(self, world_x: float, world_y: float) -> tuple[int, int]:
        """Converts world coordinates to 32x32 maze coordinates."""
        maze_x = world_x // MAZE_CELL_WORLD_SIZE
        maze_y = world_y // MAZE_CELL_WORLD_SIZE
        return int(maze_x), int(maze_y)

    def maze_to_world_coords(self, maze_x: int, maze_y: int) -> tuple[float, float]:
        """Converts maze coordinates to world coordinates (center of the 50x50 block)."""
        world_x = float(maze_x * MAZE_CELL_WORLD_SIZE + self.object_size // 2)
        world_y = float(maze_y * MAZE_CELL_WORLD_SIZE + self.object_size // 2)
        return world_x, world_y

    def _build_simplified_maze(self):
        """
        Populates the 32x32 simplified maze based on obstacle objects.
        A cell is an obstacle (1) if any part of it is occupied by an obstacle GameObject.
        This is primarily for client-side visualization of obstacles, not for pathfinding
        which is handled by the server.
        """
        try:
            # Reset maze
            self.simplified_maze = [
                [0 for _ in range(self.maze_cells_x)] for _ in range(self.maze_cells_y)
            ]

            for obj_id, obj in self.objects.items():
                if obj.is_obstacle:
                    # Determine the maze cells this obstacle covers
                    maze_start_x, maze_start_y = self.world_to_maze_coords(obj.x, obj.y)
                    maze_end_x, maze_end_y = self.world_to_maze_coords(
                        obj.x + self.object_size - 1, obj.y + self.object_size - 1
                    )

                    # Ensure coordinates are within maze bounds before marking
                    maze_start_x = max(0, min(maze_start_x, self.maze_cells_x - 1))
                    maze_start_y = max(0, min(maze_start_y, self.maze_cells_y - 1))
                    maze_end_x = max(0, min(maze_end_x, self.maze_cells_x - 1))
                    maze_end_y = max(0, min(maze_end_y, self.maze_cells_y - 1))

                    # Mark all covered maze cells as obstacles
                    for y in range(maze_start_y, maze_end_y + 1):
                        for x in range(maze_start_x, maze_end_x + 1):
                            self.simplified_maze[y][x] = 1
        except Exception as e:
            logging.exception(f"Error rebuilding simplified maze: {e}")

    def add_object(self, obj: GameObject):
        """Adds an object to the game state."""
        with self.lock:
            if obj.obj_id in self.objects:
                logging.warning(f"Object with ID {obj.obj_id} already exists.")
                return
            self.objects[obj.obj_id] = obj
            grid_x, grid_y = self._world_to_grid_coords(obj.x, obj.y)
            if 0 <= grid_x < self.grid_cells_x and 0 <= grid_y < self.grid_cells_y:
                self.grid[grid_y][grid_x] = obj
            self._build_simplified_maze()  # Rebuild maze when objects are added/removed/moved
            logging.debug(f"Added object: {obj.obj_id} at ({obj.x}, {obj.y})")

    def remove_object(self, obj_id: str):
        """Removes an object from the game state."""
        with self.lock:
            if obj_id in self.objects:
                obj = self.objects.pop(obj_id)
                grid_x, grid_y = self._world_to_grid_coords(obj.x, obj.y)
                if (
                    0 <= grid_x < self.grid_cells_x
                    and 0 <= grid_y < self.grid_cells_y
                    and self.grid[grid_y][grid_x] == obj
                ):  # Ensure it's the same object
                    self.grid[grid_y][grid_x] = None
                self._build_simplified_maze()  # Rebuild maze when objects are added/removed/moved
                logging.debug(f"Removed object: {obj_id}")
            else:
                logging.warning(f"Attempted to remove non-existent object: {obj_id}")

    def update_object_position(self, obj_id: str, new_x: float, new_y: float):
        """Updates an object's position in the game state."""
        with self.lock:
            obj = self.objects.get(obj_id)
            if not obj:
                logging.warning(
                    f"Attempted to update position of non-existent object: {obj_id}"
                )
                return

            # Clear old grid cell reference
            old_grid_x, old_grid_y = self._world_to_grid_coords(obj.x, obj.y)
            if (
                0 <= old_grid_x < self.grid_cells_x
                and 0 <= old_grid_y < self.grid_cells_y
                and self.grid[old_grid_y][old_grid_x] == obj
            ):
                self.grid[old_grid_y][old_grid_x] = None

            # Update object position
            obj.x = new_x
            obj.y = new_y

            # Set new grid cell reference
            new_grid_x, new_grid_y = self._world_to_grid_coords(new_x, new_y)
            if (
                0 <= new_grid_x < self.grid_cells_x
                and 0 <= new_grid_y < self.grid_cells_y
            ):
                self.grid[new_grid_y][new_grid_x] = obj
            self._build_simplified_maze()  # Rebuild maze if position changed
            logging.debug(f"Updated object {obj_id} to ({new_x}, {new_y})")

    def from_dict(self, data: dict):
        """Deserializes game state from a dictionary."""
        with self.lock:
            self.objects = {
                obj_id: GameObject.from_dict(obj_data)
                for obj_id, obj_data in data["objects"].items()
            }
            # Rebuild grid and maze after loading objects
            self.grid = [
                [None for _ in range(self.grid_cells_x)]
                for _ in range(self.grid_cells_y)
            ]
            for obj_id, obj in self.objects.items():
                grid_x, grid_y = self._world_to_grid_coords(obj.x, obj.y)
                if 0 <= grid_x < self.grid_cells_x and 0 <= grid_y < self.grid_cells_y:
                    self.grid[grid_y][grid_x] = obj
            self._build_simplified_maze()


class GameRenderer:
    """
    Handles all Raylib rendering logic, including camera control.
    """

    def __init__(
        self,
        screen_width: int,
        screen_height: int,
        world_width: int,
        world_height: int,
        object_size: int,
    ):
        self.screen_width = screen_width
        self.screen_height = screen_height
        self.world_width = world_width
        self.world_height = world_height
        self.object_size = object_size

        init_window(screen_width, screen_height, "Python MMO Client")
        set_target_fps(60)

        self.camera = Camera2D()
        self.camera.offset = Vector2(screen_width / 2, screen_height / 2)
        self.camera.rotation = 0.0
        self.camera.zoom = 1.0
        self.camera.target = Vector2(0, 0)  # Initial target

        self.camera_target_world = Vector2(
            0, 0
        )  # The desired world position for the camera to smoothly move to

    def begin_frame(self):
        """Starts drawing for a new frame."""
        begin_drawing()
        clear_background(RAYWHITE)
        begin_mode2d(self.camera)

    def end_frame(self):
        """Ends drawing for the current frame."""
        end_mode2d()
        end_drawing()

    def draw_grid(self):
        """Draws the 1600x1600 grid lines."""
        for x in range(0, self.world_width + 1, self.object_size):
            draw_line(x, 0, x, self.world_height, LIGHTGRAY)
        for y in range(0, self.world_height + 1, self.object_size):
            draw_line(0, y, self.world_width, y, LIGHTGRAY)

    def draw_object(self, obj: GameObject):
        """Draws a game object."""
        draw_rectangle(
            int(obj.x), int(obj.y), self.object_size, self.object_size, obj.color
        )
        # Draw outline for obstacles
        if obj.is_obstacle:
            draw_rectangle(
                int(obj.x), int(obj.y), self.object_size, self.object_size, BLACK
            )  # Fill with black
            draw_rectangle(
                int(obj.x) + 2,
                int(obj.y) + 2,
                self.object_size - 4,
                self.object_size - 4,
                obj.color,
            )  # Inner color
        else:
            draw_rectangle(
                int(obj.x), int(obj.y), self.object_size, self.object_size, obj.color
            )

    def draw_path(self, path: list[dict[str, float]]):
        """Draws the path for the player."""
        if len(path) < 2:
            return
        for i in range(len(path) - 1):
            start_point = path[i]
            end_point = path[i + 1]
            draw_line(
                int(start_point["X"]),
                int(start_point["Y"]),
                int(end_point["X"]),
                int(end_point["Y"]),
                GREEN,
            )
            draw_circle(
                int(start_point["X"]), int(start_point["Y"]), 5, GREEN
            )  # Mark path points
        draw_circle(int(path[-1]["X"]), int(path[-1]["Y"]), 5, RED)  # End point

    def draw_debug_info(self, text: str, x: int, y: int, font_size: int, color: Color):
        """Draws debug text on the screen."""
        draw_text(text, x, y, font_size, color)

    def update_camera(self, target_world_pos: Vector2):
        """Smoothly moves the camera towards the target world position."""
        # Lerp the camera target towards the desired position
        self.camera_target_world.x = (
            self.camera_target_world.x
            + (target_world_pos.x - self.camera_target_world.x) * CAMERA_SMOOTHNESS
        )
        self.camera_target_world.y = (
            self.camera_target_world.y
            + (target_world_pos.y - self.camera_target_world.y) * CAMERA_SMOOTHNESS
        )

        # Set the camera's actual target
        self.camera.target = self.camera_target_world

    def get_world_mouse_position(self) -> Vector2:
        """Converts mouse screen position to world position."""
        return get_screen_to_world2d(get_mouse_position(), self.camera)

    def window_should_close(self) -> bool:
        """Checks if the window should close."""
        return window_should_close()

    def close_window(self):
        """Closes the Raylib window."""
        close_window()


class GameClient:
    """
    The client component of the MMO, connecting to the Go server via plain WebSockets,
    rendering the game world, and sending player actions.
    """

    def __init__(self, host: str, port: int, ws_path: str):
        self.host = host
        self.port = port
        self.ws_path = ws_path
        self.websocket_url = f"ws://{self.host}:{self.port}{self.ws_path}"
        self.ws: websocket.WebSocketApp | None = (
            None  # Will hold the websocket-client instance
        )
        self.ws_thread: threading.Thread | None = (
            None  # Thread for WebSocket communication
        )

        self.game_state = GameState(
            WORLD_WIDTH, WORLD_HEIGHT, OBJECT_SIZE
        )  # Client-side copy of game state
        self.my_player_id: str | None = None
        self.my_player_obj: GameObject | None = (
            None  # Reference to my player object in game_state.objects
        )
        self.current_path_display: list[dict[str, float]] = (
            []
        )  # Path received from server to display

        self.renderer = GameRenderer(
            SCREEN_WIDTH, SCREEN_HEIGHT, WORLD_WIDTH, WORLD_HEIGHT, OBJECT_SIZE
        )

        self.message_queue: list[dict] = []  # Thread-safe queue for incoming messages
        self.message_queue_lock = threading.Lock()
        self.connection_ready_event = (
            threading.Event()
        )  # Event to signal connection readiness

    def _on_message(self, ws: websocket.WebSocketApp, message: str):
        """Handler for incoming WebSocket messages."""
        try:
            data = json.loads(message)
            with self.message_queue_lock:
                self.message_queue.append(data)
        except json.JSONDecodeError:
            logging.error(f"Failed to decode JSON message: {message}")
        except Exception as e:
            logging.exception(f"Error in on_message: {e}")

    def _on_error(self, ws: websocket.WebSocketApp, error: Exception):
        """Handler for WebSocket errors."""
        logging.error(f"WebSocket error: {error}")
        self.connection_ready_event.clear()  # Clear event on error

    def _on_close(
        self, ws: websocket.WebSocketApp, close_status_code: int, close_msg: str
    ):
        """Handler for WebSocket close event."""
        logging.info(f"Disconnected from server: {close_status_code} - {close_msg}")
        self.my_player_id = None
        self.my_player_obj = None
        self.ws = None  # Mark connection as closed
        self.connection_ready_event.clear()  # Clear event on close

    def _on_open(self, ws: websocket.WebSocketApp):
        """Handler for successful WebSocket connection."""
        logging.info("Connected to server via Plain WebSocket!")
        self.connection_ready_event.set()  # Set event to signal connection is ready

    def _process_queued_messages(self):
        """Processes messages from the queue in the main game loop."""
        with self.message_queue_lock:
            messages_to_process = list(self.message_queue)
            self.message_queue.clear()

        for data in messages_to_process:
            self._handle_server_message(data)

    def _handle_server_message(self, data: dict):
        """Dispatches incoming server messages based on their 'type' field."""
        msg_type = data.get("type")
        if msg_type == "game_state_update":
            self.game_state.from_dict(data)
            if self.my_player_id and self.my_player_id in self.game_state.objects:
                self.my_player_obj = self.game_state.objects[self.my_player_id]
            else:
                self.my_player_obj = None
            logging.debug("Game state updated.")
        elif msg_type == "player_assigned":
            self.my_player_id = data["player_id"]
            logging.info(f"Assigned player ID: {self.my_player_id} from server.")
        elif msg_type == "player_path_update":
            player_id = data["player_id"]
            path = data["path"]
            if player_id == self.my_player_id:
                self.current_path_display = path
                if self.my_player_obj:
                    self.my_player_obj.path = path
                    self.my_player_obj.path_index = 0
                logging.info(f"Received new path for my player: {len(path)} steps.")
        elif msg_type == "message":
            logging.info(f"Server message: {data.get('text', 'No message text')}")
        else:
            logging.warning(f"Unknown message type received: {msg_type} - {data}")

    def send_message(self, msg_type: str, payload: dict | None = None):
        """Sends a JSON message over the WebSocket."""
        # Check if the WebSocket connection is active and player ID is assigned before sending
        if self.ws and self.connection_ready_event.is_set() and self.my_player_id:
            message = {"type": msg_type, "data": payload if payload is not None else {}}
            try:
                self.ws.send(json.dumps(message))
            except websocket._exceptions.WebSocketConnectionClosedException:
                logging.error("WebSocket connection is closed, cannot send message.")
            except Exception as e:
                logging.exception(f"Error sending message: {e}")
        else:
            logging.warning(
                "Cannot send move request: Player not assigned yet or WebSocket not connected."
            )

    def run(self):
        """Main client loop, handles rendering and user input."""
        logging.info(f"Connecting to WebSocket server at {self.websocket_url}")

        # Create WebSocketApp instance
        self.ws = websocket.WebSocketApp(
            self.websocket_url,
            on_open=self._on_open,
            on_message=self._on_message,
            on_error=self._on_error,
            on_close=self._on_close,
        )

        # Run the WebSocketApp in a separate thread
        self.ws_thread = threading.Thread(target=self.ws.run_forever, daemon=True)
        self.ws_thread.start()

        # Wait for the connection to be ready with a timeout
        if not self.connection_ready_event.wait(timeout=5):  # Wait up to 5 seconds
            logging.error("Failed to establish WebSocket connection within timeout.")
            self.renderer.close_window()
            return

        last_frame_time = time.time()

        while not self.renderer.window_should_close():
            current_frame_time = time.time()
            delta_time = current_frame_time - last_frame_time
            last_frame_time = current_frame_time

            # Process any messages received from the WebSocket thread
            self._process_queued_messages()

            # Update client-side player movement based on received path
            if self.my_player_obj and self.my_player_obj.path:
                self.my_player_obj.update_position(delta_time)
                # If the client-side player reached the end of its path, clear it
                if self.my_player_obj.path_index >= len(self.my_player_obj.path):
                    self.my_player_obj.path = []
                    self.my_player_obj.path_index = 0
                    self.current_path_display = []  # Clear displayed path

            # --- Input Handling ---
            if is_mouse_button_pressed(MOUSE_BUTTON_LEFT):
                world_mouse_pos = self.renderer.get_world_mouse_position()
                logging.info(
                    f"Mouse clicked at world position: ({world_mouse_pos.x}, {world_mouse_pos.y})"
                )
                # The send_message function now handles the player_id check
                self.send_message(
                    "client_move_request",
                    {"target_x": world_mouse_pos.x, "target_y": world_mouse_pos.y},
                )

            # --- Rendering ---
            self.renderer.begin_frame()

            self.renderer.draw_grid()

            # Draw all objects received from the server
            for obj_id, obj in self.game_state.objects.items():
                self.renderer.draw_object(obj)

            # Draw the path for my player
            if self.my_player_obj and self.current_path_display:
                self.renderer.draw_path(self.current_path_display)

            # Update camera to follow my player
            if self.my_player_obj:
                self.renderer.update_camera(
                    Vector2(
                        self.my_player_obj.x + OBJECT_SIZE / 2,
                        self.my_player_obj.y + OBJECT_SIZE / 2,
                    )
                )
                # Display my player ID
                self.renderer.draw_debug_info(
                    f"My Player ID: {self.my_player_id}", 10, 10, 20, BLACK
                )
                self.renderer.draw_debug_info(
                    f"My Pos: ({int(self.my_player_obj.x)}, {int(self.my_player_obj.y)})",
                    10,
                    40,
                    20,
                    BLACK,
                )
            else:
                self.renderer.draw_debug_info("Connecting...", 10, 10, 20, BLACK)

            # Display FPS - Ensure get_frame_time() is not zero
            frame_time = get_frame_time()
            if frame_time > 0:
                self.renderer.draw_debug_info(
                    f"FPS: {int(1.0 / frame_time)}", 10, SCREEN_HEIGHT - 30, 20, BLACK
                )
            else:
                self.renderer.draw_debug_info(
                    "FPS: N/A", 10, SCREEN_HEIGHT - 30, 20, BLACK
                )  # Display N/A if frame_time is 0

            self.renderer.end_frame()

            # Add a small sleep to yield control, allowing Raylib to process events
            time.sleep(0.001)

        # Clean up WebSocket connection on window close
        if self.ws:
            self.ws.close()
        logging.info("WebSocket connection closed.")
        self.renderer.close_window()
        logging.info("Client window closed.")


# --- Main Execution ---
if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Python MMO Client with Plain WebSockets and Raylib."
    )
    # The --mode argument is no longer needed as this script is exclusively a client.
    parser.add_argument(
        "--host",
        type=str,
        default=SERVER_HOST,
        help=f"Server host address (default: {SERVER_HOST}).",
    )
    parser.add_argument(
        "--port",
        type=int,
        default=SERVER_PORT,
        help=f"Server port (default: {SERVER_PORT}).",
    )

    args = parser.parse_args()

    client = GameClient(args.host, args.port, WEBSOCKET_PATH)
    client.run()
