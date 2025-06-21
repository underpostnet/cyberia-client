import logging
import threading
import time
from typing import Union

from config import MAZE_CELL_WORLD_SIZE
from network_state.network_object import NetworkObject

logging.basicConfig(
    level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s"
)


class NetworkState:
    """
    Manages the state of all network objects in the world, including their positions,
    types, and interactions. It also maintains a simplified maze representation
    for pathfinding.
    """

    def __init__(self, world_width: int, world_height: int, object_size: int):
        self.world_width = world_width
        self.world_height = world_height
        self.object_size = object_size
        self.network_objects: dict[str, NetworkObject] = {}

        # Grid for spatial partitioning and simplified maze
        self.grid_cells_x = world_width // object_size
        self.grid_cells_y = world_height // object_size
        self.grid: list[list[Union[NetworkObject, None]]] = [
            [None for _ in range(self.grid_cells_x)] for _ in range(self.grid_cells_y)  # type: ignore
        ]  # type: ignore

        self.simplified_maze: list[list[int]] = []  # 0 for walkable, 1 for obstacle
        self._build_simplified_maze()  # Initial maze build

        self.lock = threading.Lock()  # Lock for thread-safe access to network_objects

    def _world_to_grid_coords(self, world_x: float, world_y: float) -> tuple[int, int]:  # type: ignore
        """Converts world coordinates to grid coordinates."""
        grid_x = int(world_x // self.object_size)
        grid_y = int(world_y // self.object_size)
        return grid_x, grid_y

    def _grid_to_world_coords(self, grid_x: int, grid_y: int) -> tuple[float, float]:
        """Converts grid coordinates to world coordinates (center of the cell)."""
        world_x = grid_x * self.object_size + self.object_size / 2
        world_y = grid_y * self.object_size + self.object_size / 2
        return world_x, world_y

    def add_or_update_network_object(self, obj: NetworkObject):
        """Adds or updates a network object in the state and its grid position."""
        self.network_objects[obj.obj_id] = obj
        grid_x, grid_y = self._world_to_grid_coords(obj.x, obj.y)
        if 0 <= grid_x < self.grid_cells_x and 0 <= grid_y < self.grid_cells_y:
            self.grid[grid_y][grid_x] = obj
        self._build_simplified_maze()  # Rebuild maze when objects change

    # type: ignore # This comment is for mypy, not part of the code
    def get_network_object(self, obj_id: str) -> Union[NetworkObject, None]:
        """Retrieves a network object by its ID."""
        return self.network_objects.get(obj_id)

    # type: ignore # This comment is for mypy, not part of the code
    def remove_network_object(self, obj_id: str) -> Union[NetworkObject, None]:
        """Removes a network object from the state and its grid position."""
        obj_to_remove = self.network_objects.pop(obj_id, None)
        if obj_to_remove:
            grid_x, grid_y = self._world_to_grid_coords(
                obj_to_remove.x, obj_to_remove.y
            )
            if (
                0 <= grid_x < self.grid_cells_x
                and 0 <= grid_y < self.grid_cells_y
                and self.grid[grid_y][grid_x] == obj_to_remove
            ):
                self.grid[grid_y][grid_x] = None
            self._build_simplified_maze()  # Rebuild maze when objects change
        return obj_to_remove

    def get_all_network_objects(self) -> dict[str, NetworkObject]:
        """Returns a copy of all network objects currently in the state."""
        return self.network_objects.copy()

    def cleanup_expired_network_objects(
        self, current_time: float
    ) -> list[tuple[str, str]]:
        """
        Removes non-persistent network objects that have exceeded their decay time.
        Returns a list of (obj_id, object_layer_id) for objects to be removed from rendering.
        """
        object_layer_ids_to_remove = []
        expired_object_ids = []
        for obj_id, obj in list(self.network_objects.items()):
            if (
                not obj.is_persistent
                and obj.decay_time is not None
                and current_time >= obj.decay_time
            ):
                expired_object_ids.append(obj_id)

        for obj_id in expired_object_ids:
            if obj_id in self.network_objects:
                obj_to_remove = self.network_objects.pop(obj_id)
                grid_x, grid_y = self._world_to_grid_coords(
                    obj_to_remove.x, obj_to_remove.y
                )
                if (
                    0 <= grid_x < self.grid_cells_x
                    and 0 <= grid_y < self.grid_cells_y
                    and self.grid[grid_y][grid_x] == obj_to_remove
                ):
                    self.grid[grid_y][grid_x] = None

                if obj_to_remove.object_layer_ids:
                    for object_layer_id in obj_to_remove.object_layer_ids:
                        object_layer_ids_to_remove.append((obj_id, object_layer_id))
        self._build_simplified_maze()  # Rebuild maze after cleanup
        return object_layer_ids_to_remove

    def _build_simplified_maze(self):
        """
        Builds a simplified 2D grid (maze) where 0 represents walkable terrain
        and 1 represents an obstacle. This is used for pathfinding.
        """
        try:
            self.simplified_maze = [
                [0 for _ in range(self.grid_cells_x)] for _ in range(self.grid_cells_y)
            ]
            for obj in self.network_objects.values():
                if obj.is_obstacle:
                    # Calculate grid cells occupied by the obstacle
                    maze_start_x = int(obj.x // MAZE_CELL_WORLD_SIZE)
                    maze_start_y = int(obj.y // MAZE_CELL_WORLD_SIZE)
                    maze_end_x = (
                        maze_start_x + (self.object_size // MAZE_CELL_WORLD_SIZE) - 1
                    )
                    maze_end_y = (
                        maze_start_y + (self.object_size // MAZE_CELL_WORLD_SIZE) - 1
                    )

                    # Clamp coordinates to grid boundaries
                    maze_start_x = max(0, min(maze_start_x, self.grid_cells_x - 1))
                    maze_start_y = max(0, min(maze_start_y, self.grid_cells_y - 1))
                    maze_end_x = max(0, min(maze_end_x, self.grid_cells_x - 1))
                    maze_end_y = max(0, min(maze_end_y, self.grid_cells_y - 1))

                    # Mark cells as obstacles
                    for y in range(maze_start_y, maze_end_y + 1):
                        for x in range(maze_start_x, maze_end_x + 1):
                            if (
                                0 <= y < self.grid_cells_y
                                and 0 <= x < self.grid_cells_x
                            ):
                                self.simplified_maze[y][x] = 1
        except Exception as e:
            logging.exception(f"Error rebuilding simplified maze: {e}")

    def update_from_dict(self, network_objects_data: dict) -> list[tuple[str, str]]:
        """
        Updates the network state based on a dictionary of network object data,
        typically received from a server or proxy.
        Handles adding new objects, updating existing ones, and removing persistent
        objects that are no longer present in the received data.
        """
        object_layer_ids_to_remove_from_rendering_system = []
        with self.lock:
            current_obj_ids = set(self.network_objects.keys())
            new_obj_ids = set(network_objects_data.keys())

            # Identify persistent objects that were removed from the new data
            removed_obj_ids = {
                obj_id
                for obj_id in current_obj_ids
                if self.network_objects[obj_id].is_persistent
                and obj_id not in new_obj_ids
            }

            for obj_id in removed_obj_ids:
                obj_to_remove = self.network_objects.get(obj_id)
                if obj_to_remove and obj_to_remove.object_layer_ids:
                    for object_layer_id in obj_to_remove.object_layer_ids:
                        object_layer_ids_to_remove_from_rendering_system.append(
                            (obj_id, object_layer_id)
                        )
                self.network_objects.pop(obj_id, None)

            # Add or update objects from the new data
            for obj_id, obj_data in network_objects_data.items():
                obj = NetworkObject.from_dict(obj_data)
                self.network_objects[obj_id] = obj

            # Rebuild grid and maze based on the updated network objects
            self.grid = [
                [None for _ in range(self.grid_cells_x)]
                for _ in range(self.grid_cells_y)
            ]
            for obj_id, obj in self.network_objects.items():
                grid_x, grid_y = self._world_to_grid_coords(obj.x, obj.y)
                if 0 <= grid_x < self.grid_cells_x and 0 <= grid_y < self.grid_cells_y:
                    self.grid[grid_y][grid_x] = obj
            self._build_simplified_maze()

        return object_layer_ids_to_remove_from_rendering_system
