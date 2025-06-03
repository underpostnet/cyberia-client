import logging
import random
import uuid

from raylibpy import Color, Vector2

from config import (
    NETWORK_OBJECT_SIZE,
    NETWORK_OBJECT_TYPE_DEFAULT_OBJECT_LAYER_IDS,
    WORLD_HEIGHT,
    WORLD_WIDTH,
)
from network_state.network_object import NetworkObject
from object_layer.object_layer_data import _load_object_layer_data

logging.basicConfig(
    level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s"
)

OBJECT_LAYER_DATA = _load_object_layer_data()


class NetworkObjectFactory:
    """Factory for creating various types of NetworkObjects."""

    def get_object_layer_data(self) -> dict:
        """Imports and returns the object layer data."""
        return OBJECT_LAYER_DATA

    def generate_initial_state_dict(self) -> dict:
        """
        Generates an initial set of network objects for the game world,
        including a player, obstacle mounds, bots.
        """
        initial_network_objects = {}

        # Generate player object
        player_id = str(uuid.uuid4())
        initial_network_objects[player_id] = NetworkObject(
            obj_id=player_id,
            x=WORLD_WIDTH / 2 - NETWORK_OBJECT_SIZE / 2,
            y=WORLD_HEIGHT / 2 - NETWORK_OBJECT_SIZE / 2,
            color=Color(0, 121, 241, 255),
            network_object_type="PLAYER",
            is_obstacle=False,
            speed=200.0,
            object_layer_ids=NETWORK_OBJECT_TYPE_DEFAULT_OBJECT_LAYER_IDS["PLAYER"],
            is_persistent=True,
        ).to_dict()

        wall_coords = []
        seen_coords = set()

        grid_cells_x = WORLD_WIDTH // NETWORK_OBJECT_SIZE
        grid_cells_y = WORLD_HEIGHT // NETWORK_OBJECT_SIZE
        grid_center_x = grid_cells_x // 2
        grid_center_y = grid_cells_y // 2

        # Define obstacle mounds in grid coordinates (start_x, start_y, width, height)
        obstacle_mounds_grid = [
            (grid_center_x - 15, grid_center_y - 10, 6, 8),
            (grid_center_x + 9, grid_center_y - 10, 6, 8),
            (grid_center_x - 3, grid_center_y - 18, 6, 8),
            (grid_center_x - 3, grid_center_y + 10, 6, 8),
            (grid_center_x - 12, grid_center_y - 2, 4, 4),
            (grid_center_x + 8, grid_center_y - 2, 4, 4),
            (grid_center_x - 2, grid_center_y - 12, 4, 4),
            (grid_center_x - 2, grid_center_y + 8, 4, 4),
            (grid_center_x - 20, grid_center_y - 20, 3, 3),
            (grid_center_x + 17, grid_center_y - 20, 3, 3),
            (grid_center_x - 20, grid_center_y + 17, 3, 3),
            (grid_center_x + 17, grid_center_y + 17, 3, 3),
            (grid_center_x - 8, grid_center_y - 8, 2, 2),
            (grid_center_x + 6, grid_center_y + 6, 2, 2),
        ]

        for start_x_grid, start_y_grid, width_grid, height_grid in obstacle_mounds_grid:
            for y_offset in range(height_grid):
                for x_offset in range(width_grid):
                    grid_x = start_x_grid + x_offset
                    grid_y = start_y_grid + y_offset

                    if 0 <= grid_x < grid_cells_x and 0 <= grid_y < grid_cells_y:
                        world_x = float(grid_x * NETWORK_OBJECT_SIZE)
                        world_y = float(grid_y * NETWORK_OBJECT_SIZE)
                        coord_tuple = (world_x, world_y)

                        if coord_tuple not in seen_coords:
                            wall_coords.append({"X": world_x, "Y": world_y})
                            seen_coords.add(coord_tuple)

        for wall_coord in wall_coords:
            wall_id = str(uuid.uuid4())
            initial_network_objects[wall_id] = NetworkObject(
                obj_id=wall_id,
                x=wall_coord["X"],
                y=wall_coord["Y"],
                color=Color(100, 100, 100, 255),
                network_object_type="WALL",
                is_obstacle=True,
                object_layer_ids=NETWORK_OBJECT_TYPE_DEFAULT_OBJECT_LAYER_IDS["WALL"],
                is_persistent=True,
            ).to_dict()

        # Generate BOT-QUEST-PROVIDERs
        # Bot 1: Default layer ID (AYLEEN)
        bot1_obj = self.generate_bot_quest_provider(
            object_layer_ids=NETWORK_OBJECT_TYPE_DEFAULT_OBJECT_LAYER_IDS[
                "BOT-QUEST-PROVIDER"
            ],
            color=Color(255, 0, 255, 255),  # Magenta
            initial_x_offset=-200,  # Offset from center for positioning
            initial_y_offset=-200,
        )
        initial_network_objects[bot1_obj.obj_id] = bot1_obj.to_dict()

        # Bot 2: PUNK layer ID
        bot2_obj = self.generate_bot_quest_provider(
            object_layer_ids=["PUNK"],  # Explicitly set PUNK as the layer ID
            color=Color(0, 255, 255, 255),  # Cyan
            initial_x_offset=200,  # Offset from center for positioning
            initial_y_offset=200,
        )
        initial_network_objects[bot2_obj.obj_id] = bot2_obj.to_dict()

        return {
            "type": "network_state_update",
            "network_objects": initial_network_objects,
        }

    def generate_bot_quest_provider(
        self,
        object_layer_ids: list[str],
        color: Color,
        initial_x_offset: float = 0.0,
        initial_y_offset: float = 0.0,
    ) -> NetworkObject:
        """
        Generates a NetworkObject representing a BOT-QUEST-PROVIDER.
        """
        bot_id = str(uuid.uuid4())

        # Random position around the center of the map with an offset
        center_x = WORLD_WIDTH / 2 + initial_x_offset
        center_y = WORLD_HEIGHT / 2 + initial_y_offset

        # Define a smaller radius for initial spawn to ensure it's not too far off
        spawn_radius = 50
        random_offset_x = random.uniform(-spawn_radius, spawn_radius)
        random_offset_y = random.uniform(-spawn_radius, spawn_radius)

        bot_x = center_x + random_offset_x
        bot_y = center_y + random_offset_y

        # Ensure bot spawns within world boundaries
        bot_x = max(0, min(bot_x, WORLD_WIDTH - NETWORK_OBJECT_SIZE))
        bot_y = max(0, min(bot_y, WORLD_HEIGHT - NETWORK_OBJECT_SIZE))

        bot_quest_provider = NetworkObject(
            obj_id=bot_id,
            x=bot_x,
            y=bot_y,
            color=color,
            network_object_type="BOT-QUEST-PROVIDER",
            is_obstacle=False,
            speed=150.0,  # Slightly slower speed than player
            object_layer_ids=object_layer_ids,
            is_persistent=True,
        )
        return bot_quest_provider

    def generate_point_path(
        self, path_coords: list[dict[str, float]], current_time: float
    ) -> list[NetworkObject]:
        """Generates a list of NetworkObjects representing points along a path."""
        path_network_objects = []
        decay_duration = 2.0
        for point in path_coords:
            obj_id = f"path_point_{uuid.uuid4()}"
            path_network_object = NetworkObject(
                obj_id=obj_id,
                x=point["X"],
                y=point["Y"],
                color=Color(0, 255, 0, 150),
                network_object_type="POINT_PATH",
                is_obstacle=False,
                speed=0.0,
                object_layer_ids=NETWORK_OBJECT_TYPE_DEFAULT_OBJECT_LAYER_IDS[
                    "POINT_PATH"
                ],
                decay_time=current_time + decay_duration,
                is_persistent=False,
            )
            path_network_objects.append(path_network_object)
        return path_network_objects

    def generate_click_pointer(
        self, x: float, y: float, current_time: float
    ) -> NetworkObject:
        """Generates a NetworkObject representing a visual click pointer."""
        obj_id = f"click_pointer_{uuid.uuid4()}"
        decay_duration = 1.0
        click_pointer = NetworkObject(
            obj_id=obj_id,
            x=x,
            y=y,
            color=Color(255, 255, 0, 200),
            network_object_type="CLICK_POINTER",
            is_obstacle=False,
            speed=0.0,
            object_layer_ids=NETWORK_OBJECT_TYPE_DEFAULT_OBJECT_LAYER_IDS[
                "CLICK_POINTER"
            ],
            decay_time=current_time + decay_duration,
            is_persistent=False,
        )
        return click_pointer
