import logging
import threading
import time

from config import MAZE_CELL_WORLD_SIZE, OBJECT_TYPE_TO_DISPLAY_IDS
from core.game_object import GameObject


# --- Logging Configuration ---
logging.basicConfig(
    level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s"
)


class GameState:
    """
    Manages the overall state of the instance world on the client.
    The server is the authoritative source of truth for server-priority objects;
    client-side objects are managed locally.
    This class holds all active GameObjects and provides methods for updating
    the state based on server messages and managing client-side visuals.
    """

    def __init__(self, world_width: int, world_height: int, object_size: int):
        """
        Initializes the GameState.

        Args:
            world_width (int): The total width of the game world in pixels.
            world_height (int): The total height of the game world in pixels.
            object_size (int): The size (width/height) of a single grid cell/object.
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

        # Simplified maze for pathfinding (1 = obstacle, 0 = walkable)
        self.simplified_maze: list[list[int]] = [
            [0 for _ in range(self.grid_cells_x)] for _ in range(self.grid_cells_y)
        ]
        self._build_simplified_maze()  # Initial build

        self.lock = threading.Lock()  # Lock for thread-safe access to game state

    def _world_to_grid_coords(self, world_x: float, world_y: float) -> tuple[int, int]:
        """Converts world coordinates to grid coordinates."""
        grid_x = int(world_x // self.object_size)
        grid_y = int(world_y // self.object_size)
        return grid_x, grid_y

    def _grid_to_world_coords(self, grid_x: int, grid_y: int) -> tuple[float, float]:
        """Converts grid coordinates to world coordinates (center of the cell)."""
        world_x = grid_x * self.object_size + self.object_size / 2
        world_y = grid_y * self.object_size + self.object_size / 2
        return world_x, world_y

    def add_or_update_game_object(self, obj: GameObject):
        """
        Adds a new game object or updates an existing one.
        Updates the internal objects dictionary and the grid.
        Assumes the caller has acquired self.lock.
        """
        self.objects[obj.obj_id] = obj

        # Update grid for all objects, as they occupy a space
        grid_x, grid_y = self._world_to_grid_coords(obj.x, obj.y)
        if 0 <= grid_x < self.grid_cells_x and 0 <= grid_y < self.grid_cells_y:
            self.grid[grid_y][grid_x] = obj
        # NOTE: _build_simplified_maze is no longer called here.
        # It's called once after a full state update in from_dict.

    def get_game_object(self, obj_id: str) -> GameObject | None:
        """Retrieves a game object by its ID. Assumes the caller has acquired self.lock."""
        return self.objects.get(obj_id)

    def remove_game_object(self, obj_id: str) -> GameObject | None:
        """
        Removes a game object by its ID and returns the removed object.
        Clears the object from the grid. Assumes the caller has acquired self.lock.
        """
        obj_to_remove = self.objects.pop(obj_id, None)

        if obj_to_remove:
            logging.info(f"Removed object: {obj_id}")
            # Clear from grid
            grid_x, grid_y = self._world_to_grid_coords(
                obj_to_remove.x, obj_to_remove.y
            )
            if (
                0 <= grid_x < self.grid_cells_x
                and 0 <= grid_y < self.grid_cells_y
                and self.grid[grid_y][grid_x] == obj_to_remove
            ):
                self.grid[grid_y][grid_x] = None
        else:
            logging.warning(f"Attempted to remove non-existent object: {obj_id}")

        return obj_to_remove

    def get_all_objects(self) -> dict[str, GameObject]:
        """Returns a copy of all active game objects. Assumes the caller has acquired self.lock."""
        return self.objects.copy()

    def cleanup_expired_objects(self, current_time: float) -> list[tuple[str, str]]:
        """
        Removes expired client-side objects (those not server-priority and with a decay_time).
        Returns a list of (obj_id, display_id) tuples for animations to be removed
        from the rendering system. Assumes the caller has acquired self.lock.
        """
        animations_to_remove = []
        expired_object_ids = []
        for obj_id, obj in list(self.objects.items()):  # Iterate over a copy
            # Only clean up objects that are NOT server-priority and have a decay time set
            if (
                not obj.server_priority
                and obj.decay_time is not None
                and current_time >= obj.decay_time
            ):
                expired_object_ids.append(obj_id)

        for obj_id in expired_object_ids:
            if (
                obj_id in self.objects
            ):  # Check again in case it was removed by another thread/operation
                obj_to_remove = self.objects.pop(obj_id)
                logging.debug(f"Cleaning up expired client-side object: {obj_id}")
                # Clear from grid for decaying objects
                grid_x, grid_y = self._world_to_grid_coords(
                    obj_to_remove.x, obj_to_remove.y
                )
                if (
                    0 <= grid_x < self.grid_cells_x
                    and 0 <= grid_y < self.grid_cells_y
                    and self.grid[grid_y][grid_x] == obj_to_remove
                ):
                    self.grid[grid_y][grid_x] = None

                if obj_to_remove.display_ids:  # Changed from animation_asset_ids
                    for (
                        display_id
                    ) in obj_to_remove.display_ids:  # Changed from animation_asset_ids
                        animations_to_remove.append(
                            (obj_id, display_id)
                        )  # Changed from animation_asset_id
        return animations_to_remove

    def _build_simplified_maze(self):
        """
        Rebuilds the simplified maze grid based on current obstacles.
        This is called whenever the authoritative object state changes significantly.
        Assumes the caller has acquired self.lock.
        """
        try:
            # Initialize all cells as walkable (0)
            self.simplified_maze = [
                [0 for _ in range(self.grid_cells_x)] for _ in range(self.grid_cells_y)
            ]

            # Mark cells covered by obstacles as 1
            for obj in self.objects.values():
                if obj.is_obstacle:
                    maze_start_x = int(obj.x // MAZE_CELL_WORLD_SIZE)
                    maze_start_y = int(obj.y // MAZE_CELL_WORLD_SIZE)

                    maze_end_x = (
                        maze_start_x + (self.object_size // MAZE_CELL_WORLD_SIZE) - 1
                    )
                    maze_end_y = (
                        maze_start_y + (self.object_size // MAZE_CELL_WORLD_SIZE) - 1
                    )

                    maze_start_x = max(0, min(maze_start_x, self.grid_cells_x - 1))
                    maze_start_y = max(0, min(maze_start_y, self.grid_cells_y - 1))
                    maze_end_x = max(0, min(maze_end_x, self.grid_cells_x - 1))
                    maze_end_y = max(0, min(maze_end_y, self.grid_cells_y - 1))

                    for y in range(maze_start_y, maze_end_y + 1):
                        for x in range(maze_start_x, maze_end_x + 1):
                            self.simplified_maze[y][x] = 1
        except Exception as e:
            logging.exception(f"Error rebuilding simplified maze: {e}")

    def from_dict(self, data: dict) -> list[tuple[str, str]]:
        """
        Deserializes and updates the client's instance state from a dictionary
        received from the server. This is the only way the client's instance state
        should be modified for server-priority objects.

        Args:
            data (dict): A dictionary representing the full instance state from the server.

        Returns:
            list[tuple[str, str]]: A list of (obj_id, display_id) tuples for animations
                                    to be removed from the rendering system.
        """
        animations_to_remove_from_rendering_system = []
        with self.lock:
            # Identify server-authoritative objects that are no longer present
            current_server_obj_ids = {
                obj_id for obj_id, obj in self.objects.items() if obj.server_priority
            }
            new_server_obj_ids = set(data["objects"].keys())
            removed_server_obj_ids = current_server_obj_ids - new_server_obj_ids

            # Collect animations to remove from rendering system due to server-side despawn
            for obj_id in removed_server_obj_ids:
                obj_to_remove = self.objects.get(obj_id)
                if (
                    obj_to_remove and obj_to_remove.display_ids
                ):  # Changed from animation_asset_ids
                    for (
                        display_id
                    ) in obj_to_remove.display_ids:  # Changed from animation_asset_ids
                        animations_to_remove_from_rendering_system.append(
                            (obj_id, display_id)  # Changed from animation_asset_id
                        )
                self.objects.pop(obj_id, None)  # Directly remove from objects here

            # Update or add server-authoritative objects
            for obj_id, obj_data in data["objects"].items():
                # GameObject.from_dict now handles default display_ids if not provided by server
                obj = GameObject.from_dict(obj_data)
                self.objects[obj_id] = obj

            # Rebuild grid and maze once after all server-authoritative objects are processed
            self.grid = [
                [None for _ in range(self.grid_cells_x)]
                for _ in range(self.grid_cells_y)
            ]
            for obj_id, obj in self.objects.items():
                grid_x, grid_y = self._world_to_grid_coords(obj.x, obj.y)
                if 0 <= grid_x < self.grid_cells_x and 0 <= grid_y < self.grid_cells_y:
                    self.grid[grid_y][grid_x] = obj
            self._build_simplified_maze()  # Call once at the end

        return animations_to_remove_from_rendering_system
