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

# Define player spawn points (world coordinates) for each channel
# These points should be manually verified to be clear of obstacles for their respective channel's wall layout.
CHANNEL_ALPHA_PLAYER_SPAWNS = [
    (WORLD_WIDTH / 2 + 150, WORLD_HEIGHT / 2 - NETWORK_OBJECT_SIZE / 2),
    (WORLD_WIDTH / 2 - 150, WORLD_HEIGHT / 2 + 50 - NETWORK_OBJECT_SIZE / 2),
    (WORLD_WIDTH / 2, WORLD_HEIGHT / 2 - 150 - NETWORK_OBJECT_SIZE / 2),
]
CHANNEL_BETA_PLAYER_SPAWNS = [
    (WORLD_WIDTH / 4, WORLD_HEIGHT / 4 - NETWORK_OBJECT_SIZE / 2),
    (WORLD_WIDTH * 3/4 - NETWORK_OBJECT_SIZE, WORLD_HEIGHT / 4 - NETWORK_OBJECT_SIZE / 2), # Adjusted to avoid edge cases
    (WORLD_WIDTH / 2, WORLD_HEIGHT * 3/4 - NETWORK_OBJECT_SIZE / 2),
]

DEFAULT_PLAYER_SPAWN = (WORLD_WIDTH / 2 - NETWORK_OBJECT_SIZE / 2, WORLD_HEIGHT / 2 - NETWORK_OBJECT_SIZE / 2)


class NetworkObjectFactory:
    """Factory for creating various types of NetworkObjects."""

    def get_object_layer_data(self) -> dict:
        """Imports and returns the object layer data."""
        return OBJECT_LAYER_DATA.copy() # Return a copy to prevent modification of the original

    def _get_channel_specific_obstacle_mounds(self, channel_id: str) -> list[tuple[int, int, int, int]]:
        """Returns obstacle mound configurations for a specific channel."""
        grid_cells_x = WORLD_WIDTH // NETWORK_OBJECT_SIZE
        grid_cells_y = WORLD_HEIGHT // NETWORK_OBJECT_SIZE
        grid_center_x = grid_cells_x // 2
        grid_center_y = grid_cells_y // 2

        # Define obstacle mounds in grid coordinates (start_x, start_y, width, height)
        obstacle_mounds_grid_default = [ # Renamed from obstacle_mounds_grid to be explicit
            (grid_center_x - 15, grid_center_y - 10, 6, 8), # Default/Channel Alpha
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

        obstacle_mounds_grid_beta = [
            (grid_center_x - 10, grid_center_y - 15, 8, 6), # Channel Beta - different layout
            (grid_center_x + 2, grid_center_y - 15, 8, 6),
            (grid_center_x - 18, grid_center_y - 3, 8, 6),
            (grid_center_x + 10, grid_center_y - 3, 8, 6),
            (grid_center_x - 5, grid_center_y - 5, 10, 10), # Central block
        ]

        if channel_id == "channel_beta":
            return obstacle_mounds_grid_beta
        return obstacle_mounds_grid_default # Corrected variable name

    def _world_to_grid_coords(self, world_x: float, world_y: float) -> tuple[int, int]:
        """Converts world coordinates (top-left of object) to grid cell indices."""
        grid_x = int(world_x // NETWORK_OBJECT_SIZE)
        grid_y = int(world_y // NETWORK_OBJECT_SIZE)
        return grid_x, grid_y

    def _grid_to_world_coords(self, grid_x: int, grid_y: int) -> tuple[float, float]:
        """Converts grid cell indices to world coordinates (top-left of the cell)."""
        world_x = float(grid_x * NETWORK_OBJECT_SIZE)
        world_y = float(grid_y * NETWORK_OBJECT_SIZE)
        return world_x, world_y

    def _find_valid_spawn_location_in_grid(
        self,
        grid_maze: list[list[int]],
        preferred_grid_x: int,
        preferred_grid_y: int,
        search_radius_grid_cells: int = 10,
        max_attempts: int = 50,
    ) -> tuple[int, int] | None:
        """
        Finds a valid (non-obstacle, within bounds) grid location for spawning.
        Starts with preferred_grid_x, preferred_grid_y, then searches randomly within a radius.
        Returns (grid_x, grid_y) or None if no valid spot is found.
        """
        grid_cells_y = len(grid_maze)
        grid_cells_x = len(grid_maze[0]) if grid_cells_y > 0 else 0

        # Check preferred location first
        if 0 <= preferred_grid_y < grid_cells_y and \
           0 <= preferred_grid_x < grid_cells_x and \
           grid_maze[preferred_grid_y][preferred_grid_x] == 0:
            return preferred_grid_x, preferred_grid_y

        # Search randomly within radius
        for _ in range(max_attempts):
            offset_x = random.randint(-search_radius_grid_cells, search_radius_grid_cells)
            offset_y = random.randint(-search_radius_grid_cells, search_radius_grid_cells)
            test_x = preferred_grid_x + offset_x
            test_y = preferred_grid_y + offset_y

            if 0 <= test_y < grid_cells_y and \
               0 <= test_x < grid_cells_x and \
               grid_maze[test_y][test_x] == 0:
                return test_x, test_y

        logging.warning(f"Could not find a valid spawn location near ({preferred_grid_x},{preferred_grid_y}) after {max_attempts} attempts.")
        return None # Fallback: no valid spot found

    def generate_initial_state_dict(self, channel_id: str = "channel_alpha") -> dict:
        """
        Generates an initial set of network objects for the game world based on the channel_id,
        including a player, obstacle mounds, and bots.
        """
        initial_network_objects = {}
        wall_coords = []
        seen_coords = set()
        grid_cells_x = WORLD_WIDTH // NETWORK_OBJECT_SIZE
        grid_cells_y = WORLD_HEIGHT // NETWORK_OBJECT_SIZE
        obstacle_mounds_grid = self._get_channel_specific_obstacle_mounds(channel_id)

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
        
        # Build a temporary maze based on the generated walls for bot placement
        temp_maze_for_bots = [[0 for _ in range(grid_cells_x)] for _ in range(grid_cells_y)]
        for wall_coord_dict in wall_coords:
            wall_x_coord = wall_coord_dict["X"]
            wall_y_coord = wall_coord_dict["Y"]
            # Convert wall's top-left world coord to grid coord
            gx, gy = self._world_to_grid_coords(wall_x_coord, wall_y_coord)
            if 0 <= gx < grid_cells_x and 0 <= gy < grid_cells_y:
                temp_maze_for_bots[gy][gx] = 1 # Mark as obstacle

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

        # Generate player object with channel-specific spawn
        player_id = str(uuid.uuid4())
        player_spawn_x, player_spawn_y = DEFAULT_PLAYER_SPAWN # Fallback

        if channel_id == "channel_beta":
            if CHANNEL_BETA_PLAYER_SPAWNS:
                player_spawn_x, player_spawn_y = random.choice(CHANNEL_BETA_PLAYER_SPAWNS)
        elif channel_id == "channel_alpha": # Default or alpha
            if CHANNEL_ALPHA_PLAYER_SPAWNS:
                player_spawn_x, player_spawn_y = random.choice(CHANNEL_ALPHA_PLAYER_SPAWNS)
        
        # Validate chosen player spawn point (simple check, assumes pre-vetted points are mostly fine)
        player_grid_x, player_grid_y = self._world_to_grid_coords(player_spawn_x, player_spawn_y)
        if not (0 <= player_grid_y < grid_cells_y and \
                0 <= player_grid_x < grid_cells_x and \
                temp_maze_for_bots[player_grid_y][player_grid_x] == 0):
            logging.warning(f"Player spawn ({player_spawn_x},{player_spawn_y}) for channel {channel_id} is on an obstacle or out of bounds. Using default.")
            player_spawn_x, player_spawn_y = DEFAULT_PLAYER_SPAWN

        initial_network_objects[player_id] = NetworkObject(
            obj_id=player_id,
            x=player_spawn_x,
            y=player_spawn_y,
            color=Color(0, 121, 241, 255),
            network_object_type="PLAYER",
            is_obstacle=False,
            speed=200.0,
            object_layer_ids=NETWORK_OBJECT_TYPE_DEFAULT_OBJECT_LAYER_IDS["PLAYER"],
            is_persistent=True,
        ).to_dict()

        # Channel-specific BOT-QUEST-PROVIDERs
        if channel_id == "channel_beta":
            bot_beta1_obj = self.generate_bot_quest_provider(
                grid_maze=temp_maze_for_bots,
                object_layer_ids=["RAVE_2"], # Renamed from CYBER_VENDOR
                color=Color(255, 165, 0, 255),  # Orange
                initial_x_offset=-300,
                initial_y_offset=100,
            )
            initial_network_objects[bot_beta1_obj.obj_id] = bot_beta1_obj.to_dict()

            bot_beta2_obj = self.generate_bot_quest_provider(
                grid_maze=temp_maze_for_bots,
                object_layer_ids=["RAVE_3"], # Renamed from INFO_BROKER
                color=Color(128, 0, 128, 255),  # Purple
                initial_x_offset=150,
                initial_y_offset=-250,
            )
            initial_network_objects[bot_beta2_obj.obj_id] = bot_beta2_obj.to_dict()
        else: # Default to channel_alpha bots
            bot_alpha1_obj = self.generate_bot_quest_provider(
                grid_maze=temp_maze_for_bots,
                object_layer_ids=NETWORK_OBJECT_TYPE_DEFAULT_OBJECT_LAYER_IDS[
                    "BOT-QUEST-PROVIDER"
                ],
                color=Color(255, 0, 255, 255),  # Magenta
                initial_x_offset=-200,
                initial_y_offset=-200,
            )
            initial_network_objects[bot_alpha1_obj.obj_id] = bot_alpha1_obj.to_dict()

            bot_alpha2_obj = self.generate_bot_quest_provider(
                grid_maze=temp_maze_for_bots,
                object_layer_ids=["PUNK"],
                color=Color(0, 255, 255, 255),  # Cyan
                initial_x_offset=200,
                initial_y_offset=200,
            )
            initial_network_objects[bot_alpha2_obj.obj_id] = bot_alpha2_obj.to_dict()

        return {
            "type": "network_state_update",
            "network_objects": initial_network_objects,
            "channel_id": channel_id, # Include channel_id in the state update
        }

    def generate_bot_quest_provider(
        self,
        grid_maze: list[list[int]], # Maze for validation
        object_layer_ids: list[str],
        color: Color,
        initial_x_offset: float = 0.0,
        initial_y_offset: float = 0.0,
    ) -> NetworkObject:
        """
        Generates a NetworkObject representing a BOT-QUEST-PROVIDER.
        """
        bot_id = str(uuid.uuid4())

        # Preferred spawn center in world coordinates
        preferred_world_x = WORLD_WIDTH / 2 + initial_x_offset
        preferred_world_y = WORLD_HEIGHT / 2 + initial_y_offset

        # Convert preferred world coordinates to grid coordinates
        preferred_grid_x, preferred_grid_y = self._world_to_grid_coords(preferred_world_x, preferred_world_y)

        # Find a valid spawn location in the grid
        # Search radius in grid cells, e.g., 10 cells around preferred
        valid_grid_coords = self._find_valid_spawn_location_in_grid(
            grid_maze, preferred_grid_x, preferred_grid_y, search_radius_grid_cells=15
        )

        if valid_grid_coords:
            bot_grid_x, bot_grid_y = valid_grid_coords
            bot_x, bot_y = self._grid_to_world_coords(bot_grid_x, bot_grid_y)
        else:
            # Fallback: if no valid spot found, spawn at preferred world location (might be on obstacle)
            logging.warning(f"Bot {bot_id} could not find a clear spawn point. Spawning at preferred offset, which might be an obstacle.")
            bot_x = max(0, min(preferred_world_x, WORLD_WIDTH - NETWORK_OBJECT_SIZE))
            bot_y = max(0, min(preferred_world_y, WORLD_HEIGHT - NETWORK_OBJECT_SIZE))


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
