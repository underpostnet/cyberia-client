import logging
import math

from raylibpy import Color

from config import NETWORK_OBJECT_TYPE_DEFAULT_OBJECT_LAYER_IDS

logging.basicConfig(
    level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s"
)


class NetworkObject:
    """
    Represents an object in the network state, with properties like position, color,
    type, movement path, and persistence.
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
        object_layer_ids: list[str] | None = None,
        decay_time: float | None = None,
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

        self._decay_time = decay_time  # Timestamp when this object should decay
        self.is_persistent = (
            is_persistent  # If True, object is not automatically removed
        )

    @property
    def decay_time(self) -> float | None:
        """Returns the decay time of the object."""
        return self._decay_time

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
        return obj

    def to_dict(self) -> dict:
        """Converts the NetworkObject instance to a dictionary."""
        return {
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
