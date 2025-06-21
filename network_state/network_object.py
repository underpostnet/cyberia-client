import logging
import math
import random
from typing import (
    Union,
    Optional,
)  # Added Optional for clarity, though Union handles it
import time

from pyray import Color, Vector2

from config import (
    NETWORK_OBJECT_TYPE_DEFAULT_OBJECT_LAYER_IDS,
    NETWORK_OBJECT_SIZE,
    WORLD_WIDTH,
    WORLD_HEIGHT,
)

logging.basicConfig(
    level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s"
)


class NetworkObject:
    """
    Represents an object in the network state, with properties like position, color,
    type, movement path, and persistence. Can also be configured for autonomous movement.
    """

    def __init__(
        self,
        obj_id: str,
        x: float,
        y: float,
        color: Color,
        is_obstacle: bool = False,
        speed: float = 200.0,
        network_object_type: str = "UNKNOWN",
        object_layer_ids: Union[list[str], None] = None,
        decay_time: Union[float, None] = None,
        is_persistent: bool = True,
    ):
        self.obj_id = obj_id
        self.x = x
        self.y = y
        self.color = color
        self.is_obstacle = is_obstacle
        self.speed = speed
        self.network_object_type = network_object_type.upper()

        # Assign default object layer IDs if not provided
        self.object_layer_ids = (
            object_layer_ids
            if object_layer_ids is not None
            else NETWORK_OBJECT_TYPE_DEFAULT_OBJECT_LAYER_IDS.get(
                self.network_object_type, []
            )
        )

        self.path: list[dict[str, float]] = (
            []
        )  # List of {'X': float, 'Y': float} for movement
        self.path_index = 0  # Current index in the path

        self._prev_x = x  # Previous X position for movement calculation
        self._prev_y = y  # Previous Y position for movement calculation

        self._decay_time: Union[float, None] = (
            decay_time  # Timestamp when this object should decay
        )
        self.is_persistent = (
            is_persistent  # If True, object is not automatically removed
        )

        # Autonomous movement attributes (specific to BOT-QUEST-PROVIDER)
        self._initial_pos: Union[Vector2, None] = None
        self._wander_radius: float = 0.0
        self._path_cooldown: float = 0.0
        self._last_path_time: float = 0.0

    @property  # type: ignore
    def decay_time(self) -> Union[float, None]:
        """Returns the decay time of the object."""
        return self._decay_time

    @decay_time.setter  # type: ignore
    def decay_time(self, value: Union[float, None]):
        """Sets the decay time of the object."""
        self._decay_time = value

    def set_path(self, path: list[dict[str, float]]):
        """Sets a new movement path for the object."""
        self.path = path
        self.path_index = 0

    def update_position(self, delta_time: float) -> tuple[float, float]:
        """
        Updates the object's position along its path based on delta_time and speed.
        Returns the change in x and y position.
        """
        prev_x, prev_y = self.x, self.y

        if not self.path or self.path_index >= len(self.path):
            self._prev_x = self.x
            self._prev_y = self.y
            return 0.0, 0.0  # No movement if no path or path completed

        target_point = self.path[self.path_index]
        target_world_x = target_point["X"]
        target_world_y = target_point["Y"]

        dx_to_target = target_world_x - self.x
        dy_to_target = target_world_y - self.y
        distance_to_target = math.sqrt(
            dx_to_target * dx_to_target + dy_to_target * dy_to_target
        )

        move_distance = self.speed * delta_time

        if distance_to_target < move_distance:
            # Reached or overshot the current target point, move to next
            self.x = target_world_x
            self.y = target_world_y
            self.path_index += 1
        else:
            # Move towards the target point
            direction_x = dx_to_target / distance_to_target
            direction_y = dy_to_target / distance_to_target
            self.x += direction_x * move_distance
            self.y += direction_y * move_distance

        current_dx = self.x - prev_x
        current_dy = self.y - prev_y

        self._prev_x = self.x
        self._prev_y = self.y

        return current_dx, current_dy

    def configure_autonomous_movement(
        self,
        initial_pos: Vector2,
        wander_radius: float = 30.0,
        path_cooldown: float = 5.0,
    ):
        """
        Configures this network object for autonomous movement.
        This method is intended for objects like 'BOT-QUEST-PROVIDER'.
        Args:
            initial_pos: The initial position around which the agent will wander.
            wander_radius: The maximum radius for wandering.
            path_cooldown: The cooldown time before generating a new path.
        """
        self._initial_pos = initial_pos
        self._wander_radius = wander_radius
        self._path_cooldown = path_cooldown
        self._last_path_time = 0.0  # Initialize cooldown

    def update_autonomous_movement(
        self,
        offline_network_state,  # Type hint cannot be NetworkState due to circular import
        current_time: float,
        delta_time: float,
        send_to_client_callback,
    ) -> None:
        """
        Updates the autonomous agent's position and path based on its strategy.
        This method should only be called if network_object_type is 'BOT-QUEST-PROVIDER'.
        """
        if self.network_object_type != "BOT-QUEST-PROVIDER":
            return

        # Update object's position along its current path
        if self.path:
            self.update_position(delta_time)
            # If path completed, clear it
            if self.path_index >= len(self.path):
                self.path = []
                self.path_index = 0
                self._last_path_time = current_time  # Mark time for cooldown

        # Assign a new path if current path is complete and cooldown is over
        if (
            not self.path
            and (current_time - self._last_path_time) >= self._path_cooldown
        ):
            # Find a new random target within the radius of the initial position
            initial_x, initial_y = self._initial_pos.x, self._initial_pos.y

            # Try to find a valid target location that is not on an obstacle
            max_attempts = 10
            found_valid_target = False
            target_x, target_y = (
                self.x,
                self.y,
            )  # Default to current position if no valid target found

            for attempt in range(max_attempts):
                angle = random.uniform(0, 2 * math.pi)
                distance = random.uniform(0, self._wander_radius)

                potential_target_x = initial_x + distance * math.cos(angle)
                potential_target_y = initial_y + distance * math.sin(angle)

                # Clamp potential target coordinates to world boundaries
                potential_target_x = max(
                    0, min(potential_target_x, WORLD_WIDTH - NETWORK_OBJECT_SIZE)
                )
                potential_target_y = max(
                    0, min(potential_target_y, WORLD_HEIGHT - NETWORK_OBJECT_SIZE)
                )

                target_grid_x, target_grid_y = (
                    offline_network_state._world_to_grid_coords(
                        potential_target_x, potential_target_y
                    )
                )

                # Check if the target grid cell is within bounds and not an obstacle
                if (
                    0 <= target_grid_x < offline_network_state.grid_cells_x
                    and 0 <= target_grid_y < offline_network_state.grid_cells_y
                    and offline_network_state.simplified_maze[target_grid_y][
                        target_grid_x
                    ]
                    == 0
                ):
                    target_x, target_y = potential_target_x, potential_target_y
                    found_valid_target = True
                    break

            if not found_valid_target:
                logging.warning(
                    f"Autonomous agent {self.obj_id}: Could not find a valid non-obstacle target after {max_attempts} attempts. Staying put for this cycle."
                )
                self._last_path_time = current_time  # Reset cooldown to try again
                return  # Exit if no valid target found

            start_grid_x, start_grid_y = offline_network_state._world_to_grid_coords(
                self.x, self.y
            )
            end_grid_x, end_grid_y = offline_network_state._world_to_grid_coords(
                target_x, target_y
            )

            maze = offline_network_state.simplified_maze
            # Ensure astar is imported or passed
            from network_state.astar import astar

            path_grid = astar(
                maze, (start_grid_y, start_grid_x), (end_grid_y, end_grid_x)
            )

            if path_grid:
                path_world_coords = []
                for grid_y, grid_x in path_grid:
                    world_x, world_y = offline_network_state._grid_to_world_coords(
                        grid_x, grid_y
                    )
                    path_world_coords.append({"X": world_x, "Y": world_y})

                self.set_path(path_world_coords)
                logging.info(
                    f"Autonomous agent {self.obj_id} assigned new path with {len(path_world_coords)} points."
                )

                # Broadcast agent's path update to client
                send_to_client_callback(
                    {
                        "type": "player_path_update",  # Re-use player_path_update for bot movement
                        "player_id": self.obj_id,
                        "path": path_world_coords,
                    }
                )
            else:
                logging.warning(
                    f"No path found for autonomous agent {self.obj_id} from ({start_grid_x},{start_grid_y}) to ({end_grid_x},{end_grid_y}). This might indicate an unreachable area or a bug in pathfinding."
                )
                # If no path found, reset cooldown to try again soon
                self._last_path_time = current_time

    def get_display_label(self) -> Optional[str]:
        """
        Returns a string label to be displayed above the network object,
        or None if no label should be displayed.
        """
        if self.network_object_type == "PLAYER":
            return f"{self.obj_id}"
        elif self.network_object_type == "BOT-QUEST-PROVIDER":
            if self.object_layer_ids and len(self.object_layer_ids) > 0:
                return self.object_layer_ids[0]
        return None  # No label for other types or if conditions not met

    @classmethod
    def from_dict(cls, data: dict):
        """Creates a NetworkObject instance from a dictionary."""
        obj_id = data["obj_id"]
        x = data["x"]
        y = data["y"]

        color_r = data.get("color_r", 255)
        color_g = data.get("color_g", 255)
        color_b = data.get("color_b", 255)
        color_a = data.get("color_a", 255)
        color = Color(color_r, color_g, color_b, color_a)

        is_obstacle = data.get("is_obstacle", False)
        speed = data.get("speed", 200.0)
        network_object_type = data.get("network_object_type", "UNKNOWN")
        object_layer_ids_from_server = data.get("object_layer_ids")
        decay_time = data.get("decay_time")
        is_persistent = data.get("is_persistent", True)

        obj = cls(
            obj_id=obj_id,
            x=x,
            y=y,
            color=color,
            is_obstacle=is_obstacle,
            speed=speed,
            network_object_type=network_object_type,
            object_layer_ids=object_layer_ids_from_server,
            decay_time=decay_time,
            is_persistent=is_persistent,
        )
        if "path" in data:
            obj.set_path(data["path"])

        # Load autonomous movement attributes if present and object type is BOT-QUEST-PROVIDER
        if network_object_type.upper() == "BOT-QUEST-PROVIDER":
            initial_pos_data = data.get("initial_pos")
            if initial_pos_data:
                initial_pos = Vector2(initial_pos_data["x"], initial_pos_data["y"])
            else:
                initial_pos = Vector2(
                    x, y
                )  # Default to current position if not specified

            obj.configure_autonomous_movement(
                initial_pos=initial_pos,
                wander_radius=data.get("wander_radius", 500.0),
                path_cooldown=data.get("path_cooldown", 5.0),
            )
            obj._last_path_time = data.get(
                "last_path_time", 0.0
            )  # Ensure last_path_time is loaded

        return obj

    def to_dict(self) -> dict:
        """Converts the NetworkObject instance to a dictionary."""
        data = {
            "obj_id": self.obj_id,
            "x": self.x,
            "y": self.y,
            "color_r": self.color.r,
            "color_g": self.color.g,
            "color_b": self.color.b,
            "color_a": self.color.a,
            "is_obstacle": self.is_obstacle,
            "speed": self.speed,
            "network_object_type": self.network_object_type,
            "object_layer_ids": self.object_layer_ids,
            "path": self.path,
            "decay_time": self._decay_time,
            "is_persistent": self.is_persistent,
        }
        # Include autonomous movement attributes if it's a BOT-QUEST-PROVIDER
        if self.network_object_type == "BOT-QUEST-PROVIDER":
            if self._initial_pos:
                data["initial_pos"] = {
                    "x": self._initial_pos.x,
                    "y": self._initial_pos.y,
                }
            data["wander_radius"] = self._wander_radius
            data["path_cooldown"] = self._path_cooldown
            data["last_path_time"] = (
                self._last_path_time
            )  # Ensure last_path_time is saved
        return data
