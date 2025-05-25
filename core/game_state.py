import logging
import threading

# --- Logging Configuration ---
logging.basicConfig(
    level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s"
)

from config import (
    MAZE_CELL_WORLD_SIZE,
)

from core.game_object import GameObject


class GameState:
    """
    Manages the overall state of the instance world on the client.
    The server is the authoritative source of truth; the client only reflects its state.
    This class holds all active GameObjects and provides methods for updating
    the state based on server messages.
    """

    def __init__(self, world_width: int, world_height: int, object_size: int):
        """
        Initializes the GameState.

        Args:
            world_width (int): The total width of the game world in pixels.
            world_height (int): The total height of the game world in pixels.
            object_size (int): The size (width/height) of a single game object in pixels.
        """
        self.world_width = world_width
        self.world_height = world_height
        self.object_size = object_size
        self.objects: dict[str, GameObject] = {}  # Dictionary: obj_id -> GameObject

        # Conceptual grid for object placement (for visualization and basic collisions)
        self.grid_cells_x = world_width // object_size
        self.grid_cells_y = world_height // object_size
        self.grid: list[list[GameObject | None]] = [
            [None for _ in range(self.grid_cells_x)] for _ in range(self.grid_cells_y)
        ]

        # Simplified 32x32 maze for obstacle visualization (not for pathfinding)
        self.maze_cells_x = world_width // MAZE_CELL_WORLD_SIZE
        self.maze_cells_y = world_height // MAZE_CELL_WORLD_SIZE
        self.simplified_maze: list[list[int]] = [
            [0 for _ in range(self.maze_cells_x)] for _ in range(self.maze_cells_y)
        ]
        # The maze is built in from_dict when the instance state is received from the server.

        self.lock = threading.Lock()  # To protect instance state from concurrent access

    def _world_to_grid_coords(self, world_x: float, world_y: float) -> tuple[int, int]:
        """
        Converts world coordinates to grid cell indices.

        Args:
            world_x (float): The X-coordinate in world space.
            world_y (float): The Y-coordinate in world space.

        Returns:
            tuple[int, int]: A tuple (grid_x, grid_y) representing the grid cell.
        """
        grid_x = world_x // self.object_size
        grid_y = world_y // self.object_size
        return int(grid_x), int(grid_y)

    def world_to_maze_coords(self, world_x: float, world_y: float) -> tuple[int, int]:
        """
        Converts world coordinates to 32x32 maze coordinates.
        This is used for the simplified client-side obstacle visualization.

        Args:
            world_x (float): The X-coordinate in world space.
            world_y (float): The Y-coordinate in world space.

        Returns:
            tuple[int, int]: A tuple (maze_x, maze_y) representing the maze cell.
        """
        maze_x = world_x // MAZE_CELL_WORLD_SIZE
        maze_y = world_y // MAZE_CELL_WORLD_SIZE
        return int(maze_x), int(maze_y)

    def _build_simplified_maze(self):
        """
        Populates the 32x32 simplified maze based on current obstacle objects.
        This is exclusively for client-side visualization of obstacles,
        not for calculating paths (the server handles that).
        """
        try:
            # Reset maze to all zeros (empty)
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

                    # Mark all covered maze cells as obstacles (1)
                    for y in range(maze_start_y, maze_end_y + 1):
                        for x in range(maze_start_x, maze_end_x + 1):
                            self.simplified_maze[y][x] = 1
        except Exception as e:
            logging.exception(f"Error rebuilding simplified maze: {e}")

    def from_dict(self, data: dict):
        """
        Deserializes and updates the client's instance state from a dictionary
        received from the server. This is the only way the client's instance state
        should be modified.

        Args:
            data (dict): A dictionary representing the full instance state from the server.
        """
        with self.lock:
            # Update the objects dictionary
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
            self._build_simplified_maze()  # Rebuild visualization maze based on new objects
