import argparse
import logging
import random
import time
import math
import threading

import socketio
import eventlet
from eventlet import wsgi

# Apply monkey patching for eventlet at the very beginning
eventlet.monkey_patch()

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
        self, obj_id: str, x: int, y: int, color: Color, is_obstacle: bool = False
    ):
        self.obj_id = obj_id
        self.x = x
        self.y = y
        self.color = color
        self.is_obstacle = is_obstacle
        self.target_x = x  # For smooth movement
        self.target_y = y  # For smooth movement
        self.path = []  # List of (x, y) world coordinates to follow
        self.path_index = 0
        self.speed = 200  # Pixels per second

    def to_dict(self):
        """Converts object state to a dictionary for serialization."""
        return {
            "obj_id": self.obj_id,
            "x": self.x,
            "y": self.y,
            "color": (self.color.r, self.color.g, self.color.b, self.color.a),
            "is_obstacle": self.is_obstacle,
            "target_x": self.target_x,
            "target_y": self.target_y,
            "path": self.path,
            "path_index": self.path_index,
        }

    @classmethod
    def from_dict(cls, data: dict):
        """Creates a GameObject instance from a dictionary."""
        color = Color(
            data["color"][0], data["color"][1], data["color"][2], data["color"][3]
        )
        obj = cls(data["obj_id"], data["x"], data["y"], color, data["is_obstacle"])
        obj.target_x = data.get("target_x", obj.x)
        obj.target_y = data.get("target_y", obj.y)
        obj.path = data.get("path", [])
        obj.path_index = data.get("path_index", 0)
        return obj

    def update_position(self, delta_time: float):
        """Updates the object's position, moving along its path."""
        if not self.path or self.path_index >= len(self.path):
            self.path = []
            self.path_index = 0
            return

        target_point = self.path[self.path_index]
        target_world_x = target_point[0]
        target_world_y = target_point[1]

        # Calculate distance to target
        dx = target_world_x - self.x
        dy = target_world_y - self.y
        distance = math.sqrt(dx * dx + dy * dy)

        if distance < self.speed * delta_time:
            # Reached or overshot the current target point, move to it exactly
            self.x = target_world_x
            self.y = target_world_y
            self.path_index += 1
        else:
            # Move towards the target point
            direction_x = dx / distance
            direction_y = dy / distance
            self.x += direction_x * self.speed * delta_time
            self.y += direction_y * self.speed * delta_time


class GameState:
    """
    Manages the overall state of the game world, including objects and the grid.
    Handles A* pathfinding logic.
    """

    def __init__(self, world_width: int, world_height: int, object_size: int):
        self.world_width = world_width
        self.world_height = world_height
        self.object_size = object_size
        self.objects = {}  # Dictionary: obj_id -> GameObject

        # Initialize the 1600x1600 grid (stores references to objects or None)
        # This grid is conceptual for object placement, not for A* directly.
        self.grid_cells_x = world_width // object_size
        self.grid_cells_y = world_height // object_size
        self.grid = [
            [None for _ in range(self.grid_cells_x)] for _ in range(self.grid_cells_y)
        ]

        # Initialize the 32x32 simplified maze for A*
        self.maze_cells_x = world_width // MAZE_CELL_WORLD_SIZE
        self.maze_cells_y = world_height // MAZE_CELL_WORLD_SIZE
        self.simplified_maze = [
            [0 for _ in range(self.maze_cells_x)] for _ in range(self.maze_cells_y)
        ]
        self._build_simplified_maze()  # Initial build

        self.lock = threading.Lock()  # To protect game state from concurrent access

    def _world_to_grid_coords(self, world_x: int, world_y: int):
        """Converts world coordinates to grid cell indices."""
        grid_x = world_x // self.object_size
        grid_y = world_y // self.object_size
        return int(grid_x), int(grid_y)

    def _grid_to_world_coords(self, grid_x: int, grid_y: int):
        """Converts grid cell indices to world coordinates (top-left of cell)."""
        world_x = grid_x * self.object_size
        world_y = grid_y * self.object_size
        return world_x, world_y

    def world_to_maze_coords(self, world_x: int, world_y: int):
        """Converts world coordinates to 32x32 maze coordinates."""
        maze_x = world_x // MAZE_CELL_WORLD_SIZE
        maze_y = world_y // MAZE_CELL_WORLD_SIZE
        return int(maze_x), int(maze_y)

    def maze_to_world_coords(self, maze_x: int, maze_y: int):
        """Converts maze coordinates to world coordinates (center of the 50x50 block)."""
        world_x = maze_x * MAZE_CELL_WORLD_SIZE + self.object_size // 2
        world_y = maze_y * MAZE_CELL_WORLD_SIZE + self.object_size // 2
        return world_x, world_y

    def _build_simplified_maze(self):
        """
        Populates the 32x32 simplified maze based on obstacle objects.
        A cell is an obstacle (1) if any part of it is occupied by an obstacle GameObject.
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
            # logging.info("Simplified maze rebuilt successfully.")
        except Exception as e:
            logging.exception(f"Error rebuilding simplified maze: {e}")
            # If maze building fails, it's a critical error, but we'll log it.
            # The server might still run but pathfinding would be broken.

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
            self._build_simplified_maze()
            logging.info(f"Added object: {obj.obj_id} at ({obj.x}, {obj.y})")

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
                self._build_simplified_maze()
                logging.info(f"Removed object: {obj_id}")
            else:
                logging.warning(f"Attempted to remove non-existent object: {obj_id}")

    def update_object_position(self, obj_id: str, new_x: int, new_y: int):
        """Updates an object's position in the game state."""
        with self.lock:
            if obj_id in self.objects:
                obj = self.objects[obj_id]
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
            else:
                logging.warning(
                    f"Attempted to update position of non-existent object: {obj_id}"
                )

    def get_random_available_position(self):
        """
        Finds a random available (non-obstacle) position for a new object.
        Returns world coordinates (top-left of the 50x50 cell).
        """
        available_cells = []
        for y in range(self.grid_cells_y):
            for x in range(self.grid_cells_x):
                if self.grid[y][x] is None:  # Cell is empty
                    # Check if this cell is part of an obstacle in the simplified maze
                    maze_x, maze_y = self.world_to_maze_coords(
                        x * self.object_size, y * self.object_size
                    )
                    if (
                        self.simplified_maze[maze_y][maze_x] == 0
                    ):  # It's a passable maze cell
                        available_cells.append(
                            (x * self.object_size, y * self.object_size)
                        )

        if not available_cells:
            logging.error("No available positions found on the grid!")
            return None
        return random.choice(available_cells)

    def find_path(self, start_world: tuple, end_world: tuple):
        """
        Calculates an A* path between two world coordinates.
        Converts world coords to maze coords, runs A*, converts path back to world coords.
        """
        with self.lock:
            start_maze = self.world_to_maze_coords(start_world[0], start_world[1])
            end_maze = self.world_to_maze_coords(end_world[0], end_world[1])

            # Ensure start and end are within maze bounds and are walkable
            if not (
                0 <= start_maze[0] < self.maze_cells_x
                and 0 <= start_maze[1] < self.maze_cells_y
                and self.simplified_maze[start_maze[1]][start_maze[0]] == 0
            ):
                logging.error(
                    f"Start position {start_world} (maze: {start_maze}) is invalid or an obstacle."
                )
                return []
            if not (
                0 <= end_maze[0] < self.maze_cells_x
                and 0 <= end_maze[1] < self.maze_cells_y
                and self.simplified_maze[end_maze[1]][end_maze[0]] == 0
            ):
                logging.error(
                    f"End position {end_world} (maze: {end_maze}) is invalid or an obstacle."
                )
                return []

            logging.info(f"Finding path from maze {start_maze} to {end_maze}...")
            try:
                # Call the imported astar function
                path_maze = astar(
                    self.simplified_maze,
                    start_maze,
                    end_maze,
                    allow_diagonal_movement=False,
                )
            except Exception as e:
                logging.exception(
                    f"Error during A* pathfinding from {start_maze} to {end_maze}: {e}"
                )
                return []

            if path_maze:
                # Convert maze path back to world coordinates (center of cells for smoother movement)
                path_world = [self.maze_to_world_coords(mx, my) for mx, my in path_maze]
                logging.info(f"Path found with {len(path_world)} steps.")
                return path_world
            else:
                logging.warning(f"No path found from {start_world} to {end_world}.")
                return []

    def to_dict(self):
        """Serializes the entire game state to a dictionary."""
        with self.lock:
            return {
                "objects": {
                    obj_id: obj.to_dict() for obj_id, obj in self.objects.items()
                },
                # simplified_maze is not sent to client as it's for server-side pathfinding
            }

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

        init_window(screen_width, screen_height, "Python MMO")
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

    def draw_path(self, path: list):
        """Draws the path for the player."""
        if len(path) < 2:
            return
        for i in range(len(path) - 1):
            start_point = path[i]
            end_point = path[i + 1]
            draw_line(
                int(start_point[0]),
                int(start_point[1]),
                int(end_point[0]),
                int(end_point[1]),
                GREEN,
            )
            draw_circle(
                int(start_point[0]), int(start_point[1]), 5, GREEN
            )  # Mark path points
        draw_circle(int(path[-1][0]), int(path[-1][1]), 5, RED)  # End point

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

    def get_world_mouse_position(self):
        """Converts mouse screen position to world position."""
        return get_screen_to_world2d(get_mouse_position(), self.camera)

    def window_should_close(self):
        """Checks if the window should close."""
        return window_should_close()

    def close_window(self):
        """Closes the Raylib window."""
        close_window()


class GameServer:
    """
    The server component of the MMO, managing game state and client communication.
    """

    def __init__(self, host: str, port: int):
        self.host = host
        self.port = port
        self.sio = socketio.Server(async_mode="eventlet", cors_allowed_origins="*")
        # Corrected: Use socketio.WSGIApp to wrap the Socket.IO server for WSGI
        self.app = socketio.WSGIApp(self.sio)
        self.game_state = GameState(WORLD_WIDTH, WORLD_HEIGHT, OBJECT_SIZE)
        self.connected_clients = {}  # sid -> player_id
        self.player_counter = 0

        # Add some initial obstacles
        self._add_initial_obstacles()

        # Register Socket.IO event handlers
        self.sio.on("connect", self.on_connect)
        self.sio.on("disconnect", self.on_disconnect)
        self.sio.on("client_move_request", self.on_client_move_request)

        # Start a background task for broadcasting game state
        self.sio.start_background_task(self._broadcast_game_state_loop)
        self.last_update_time = time.time()

    def _add_initial_obstacles(self):
        """Adds some static obstacles to the game world."""
        obstacle_positions = [
            (200, 200),
            (250, 200),
            (300, 200),
            (200, 250),
            (300, 250),
            (200, 300),
            (250, 300),
            (300, 300),
            (700, 700),
            (750, 700),
            (800, 700),
            (700, 750),
            (800, 750),
            (700, 800),
            (750, 800),
            (800, 800),
            (100, 500),
            (150, 500),
            (200, 500),
            (500, 100),
            (500, 150),
            (500, 200),
            (1000, 1000),
            (1050, 1000),
            (1100, 1000),
            (1000, 1050),
            (1100, 1050),
            (1000, 1100),
            (1050, 1100),
            (1100, 1100),
        ]
        for i, (x, y) in enumerate(obstacle_positions):
            obstacle_id = f"obstacle_{i}"
            obstacle = GameObject(obstacle_id, x, y, DARKGRAY, is_obstacle=True)
            self.game_state.add_object(obstacle)
        logging.info(f"Added {len(obstacle_positions)} initial obstacles.")

    def on_connect(self, sid, environ):
        """Handler for new client connections."""
        try:
            logging.info(f"Client connected: {sid}")
            self.player_counter += 1
            player_id = f"player_{self.player_counter}"
            self.connected_clients[sid] = player_id

            pos = self.game_state.get_random_available_position()
            if pos is None:
                logging.error(
                    f"Could not find available position for new player {player_id}. Disconnecting client {sid}."
                )
                self.sio.disconnect(sid)
                return

            player_obj = GameObject(player_id, pos[0], pos[1], BLUE)
            self.game_state.add_object(player_obj)

            self.sio.emit(
                "player_assigned",
                {"player_id": player_id, "x": pos[0], "y": pos[1]},
                room=sid,
            )
            logging.info(
                f"Assigned player {player_id} to client {sid} at ({pos[0]}, {pos[1]})"
            )

            self.sio.emit("game_state_update", self.game_state.to_dict())
        except Exception as e:
            logging.exception(f"Error in on_connect for SID {sid}: {e}")
            # Attempt to disconnect the client if an error occurs
            self.sio.disconnect(sid)

    def on_disconnect(self, sid):
        """Handler for client disconnections."""
        logging.info(f"Client disconnected: {sid}")
        player_id = self.connected_clients.pop(sid, None)
        if player_id:
            self.game_state.remove_object(player_id)
            # Broadcast the updated game state to all clients
            self.sio.emit("game_state_update", self.game_state.to_dict())

    def on_client_move_request(self, sid, data):
        """Handler for client movement requests."""
        try:
            player_id = self.connected_clients.get(sid)
            if not player_id:
                logging.warning(f"Move request from unknown client SID: {sid}")
                return

            target_x = data.get("target_x")
            target_y = data.get("target_y")

            player_obj = self.game_state.objects.get(player_id)
            if not player_obj:
                logging.error(f"Player object {player_id} not found for move request.")
                return

            start_pos = (int(player_obj.x), int(player_obj.y))
            end_pos = (int(target_x), int(target_y))
            path = self.game_state.find_path(
                start_pos, end_pos
            )  # find_path now has its own try-except

            if path:
                with self.game_state.lock:
                    player_obj.path = path
                    player_obj.path_index = 0
                logging.info(f"Calculated path for {player_id}: {path}")
                self.sio.emit(
                    "player_path_update",
                    {"player_id": player_id, "path": path},
                    room=sid,
                )
            else:
                logging.warning(
                    f"No path found for player {player_id} from {start_pos} to {end_pos}"
                )
                self.sio.emit(
                    "message", {"text": "No path found to that location!"}, room=sid
                )
        except Exception as e:
            logging.exception(f"Error in on_client_move_request for SID {sid}: {e}")
            self.sio.emit(
                "message",
                {"text": "An internal server error occurred during your move request."},
                room=sid,
            )

    def _broadcast_game_state_loop(self):
        """Periodically broadcasts the full game state to all connected clients."""
        while True:
            try:
                delta_time = time.time() - self.last_update_time
                self.last_update_time = time.time()

                # Update all moving objects on the server
                with self.game_state.lock:
                    for obj_id, obj in list(
                        self.game_state.objects.items()
                    ):  # Use list() for safe iteration if objects are removed
                        if obj.path:  # Only update if there's a path to follow
                            obj.update_position(delta_time)
                            # If player reached end of path, clear it
                            if obj.path_index >= len(obj.path):
                                obj.path = []
                                obj.path_index = 0
                                logging.info(f"Player {obj_id} reached destination.")

                # Broadcast the updated state
                self.sio.emit("game_state_update", self.game_state.to_dict())
            except Exception as e:
                logging.exception(f"Error in broadcast game state loop: {e}")
                # The loop will continue, but we'll know if it's crashing
            eventlet.sleep(0.05)  # Broadcast every 50ms (20 FPS updates)

    def run(self):
        """Starts the Socket.IO server."""
        logging.info(f"Starting server on {self.host}:{self.port}")
        try:
            wsgi.server(eventlet.listen((self.host, self.port)), self.app)
        except Exception as e:
            logging.error(f"Server failed to start: {e}")


class GameClient:
    """
    The client component of the MMO, connecting to the server, rendering, and sending actions.
    """

    def __init__(self, host: str, port: int):
        self.host = host
        self.port = port
        self.sio = socketio.Client()
        self.renderer = GameRenderer(
            SCREEN_WIDTH, SCREEN_HEIGHT, WORLD_WIDTH, WORLD_HEIGHT, OBJECT_SIZE
        )
        self.game_state = GameState(
            WORLD_WIDTH, WORLD_HEIGHT, OBJECT_SIZE
        )  # Client-side copy of game state
        self.my_player_id = None
        self.my_player_obj = None  # Reference to my player object in game_state.objects
        self.current_path_display = []  # Path received from server to display

        # Register Socket.IO event handlers
        self.sio.on("connect", self.on_connect)
        self.sio.on("disconnect", self.on_disconnect)
        self.sio.on("game_state_update", self.on_game_state_update)
        self.sio.on("player_assigned", self.on_player_assigned)
        self.sio.on("player_path_update", self.on_player_path_update)
        self.sio.on("message", self.on_message)

    def on_connect(self):
        """Handler for successful connection to the server."""
        logging.info("Connected to server!")

    def on_disconnect(self):
        """Handler for disconnection from the server."""
        logging.info("Disconnected from server.")
        self.my_player_id = None
        self.my_player_obj = None
        self.game_state = GameState(
            WORLD_WIDTH, WORLD_HEIGHT, OBJECT_SIZE
        )  # Reset client state

    def on_game_state_update(self, data):
        """Receives and updates the client's copy of the game state."""
        self.game_state.from_dict(data)
        if self.my_player_id and self.my_player_id in self.game_state.objects:
            self.my_player_obj = self.game_state.objects[self.my_player_id]
        else:
            self.my_player_obj = (
                None  # My player might have been removed or not yet assigned
            )
        logging.debug("Game state updated.")

    def on_player_assigned(self, data):
        """Receives the ID of the player assigned to this client."""
        self.my_player_id = data["player_id"]
        logging.info(f"Assigned player ID: {self.my_player_id}")
        # The first game_state_update will populate my_player_obj

    def on_player_path_update(self, data):
        """Receives the path for the client's player from the server."""
        player_id = data["player_id"]
        path = data["path"]
        if player_id == self.my_player_id:
            self.current_path_display = path
            if self.my_player_obj:
                # Client also updates its path to animate movement
                self.my_player_obj.path = path
                self.my_player_obj.path_index = 0
            logging.info(f"Received new path for my player: {len(path)} steps.")

    def on_message(self, data):
        """Handles general messages from the server."""
        logging.info(f"Server message: {data.get('text', 'No message text')}")

    def run(self):
        """Main client loop, handles rendering and user input."""
        try:
            self.sio.connect(f"http://{self.host}:{self.port}")
        except socketio.exceptions.ConnectionError as e:
            logging.error(f"Failed to connect to server: {e}")
            self.renderer.close_window()
            return

        last_frame_time = time.time()

        while not self.renderer.window_should_close():
            current_frame_time = time.time()
            delta_time = current_frame_time - last_frame_time
            last_frame_time = current_frame_time

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
                if self.my_player_id:
                    # Request server to move my player to the clicked location
                    self.sio.emit(
                        "client_move_request",
                        {"target_x": world_mouse_pos.x, "target_y": world_mouse_pos.y},
                    )
                else:
                    logging.warning(
                        "Cannot send move request: Player not assigned yet."
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

        self.sio.disconnect()
        self.renderer.close_window()
        logging.info("Client window closed.")


# --- Main Execution ---
if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Python MMO OOP Template with Socket.IO and Raylib."
    )
    parser.add_argument(
        "--mode",
        type=str,
        required=True,
        choices=["server", "client"],
        help="Run in 'server' or 'client' mode.",
    )
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

    if args.mode == "server":
        server = GameServer(args.host, args.port)
        server.run()
    elif args.mode == "client":
        client = GameClient(args.host, args.port)
        client.run()
    else:
        logging.error("Invalid mode specified. Use 'server' or 'client'.")
