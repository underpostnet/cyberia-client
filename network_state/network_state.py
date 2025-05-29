import logging
import threading
import time

from config import MAZE_CELL_WORLD_SIZE
from network_state.network_object import NetworkObject

logging.basicConfig(
    level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s"
)


class NetworkState:
    def __init__(self, world_width: int, world_height: int, object_size: int):
        self.world_width = world_width
        self.world_height = world_height
        self.object_size = object_size
        self.network_objects: dict[str, NetworkObject] = {}

        self.grid_cells_x = world_width // object_size
        self.grid_cells_y = world_height // object_size
        self.grid: list[list[NetworkObject | None]] = [
            [None for _ in range(self.grid_cells_x)] for _ in range(self.grid_cells_y)
        ]

        self.simplified_maze: list[list[int]] = [
            [0 for _ in range(self.grid_cells_x)] for _ in range(self.grid_cells_y)
        ]
        self._build_simplified_maze()

        self.lock = threading.Lock()

    def _world_to_grid_coords(self, world_x: float, world_y: float) -> tuple[int, int]:
        grid_x = int(world_x // self.object_size)
        grid_y = int(world_y // self.object_size)
        return grid_x, grid_y

    def _grid_to_world_coords(self, grid_x: int, grid_y: int) -> tuple[float, float]:
        world_x = grid_x * self.object_size + self.object_size / 2
        world_y = grid_y * self.object_size + self.object_size / 2
        return world_x, world_y

    def add_or_update_network_object(self, obj: NetworkObject):
        self.network_objects[obj.obj_id] = obj
        grid_x, grid_y = self._world_to_grid_coords(obj.x, obj.y)
        if 0 <= grid_x < self.grid_cells_x and 0 <= grid_y < self.grid_cells_y:
            self.grid[grid_y][grid_x] = obj

    def get_network_object(self, obj_id: str) -> NetworkObject | None:
        return self.network_objects.get(obj_id)

    def remove_network_object(self, obj_id: str) -> NetworkObject | None:
        obj_to_remove = self.network_objects.pop(obj_id, None)
        if obj_to_remove:
            logging.info(f"Removed network object: {obj_id}")
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
            logging.warning(
                f"Attempted to remove non-existent network object: {obj_id}"
            )
        return obj_to_remove

    def get_all_network_objects(self) -> dict[str, NetworkObject]:
        return self.network_objects.copy()

    def cleanup_expired_network_objects(
        self, current_time: float
    ) -> list[tuple[str, str]]:
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
                logging.debug(
                    f"Cleaning up expired client-side network object: {obj_id}"
                )
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
        return object_layer_ids_to_remove

    def _build_simplified_maze(self):
        try:
            self.simplified_maze = [
                [0 for _ in range(self.grid_cells_x)] for _ in range(self.grid_cells_y)
            ]
            for obj in self.network_objects.values():
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

    def update_from_dict(self, network_objects_data: dict) -> list[tuple[str, str]]:
        object_layer_ids_to_remove_from_rendering_system = []
        with self.lock:
            current_obj_ids = set(self.network_objects.keys())
            # Corrected: data is already the network_objects_data dictionary
            new_obj_ids = set(network_objects_data.keys())

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

            for obj_id, obj_data in network_objects_data.items():
                obj = NetworkObject.from_dict(obj_data)
                self.network_objects[obj_id] = obj

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
