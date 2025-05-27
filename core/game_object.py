import logging
import math

from raylibpy import Color

# Import object type constants and display ID mapping for consistency
from config import (
    SERVER_PRIORITY_OBJECT_TYPES,
    OBJECT_TYPE_TO_DISPLAY_IDS,
)

# --- Logging Configuration ---
logging.basicConfig(
    level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s"
)


class GameObject:
    """
    Represents a generic object within an instance world.
    This object is a local representation of the server's state,
    and includes properties for position, color, obstacle status,
    speed, and pathfinding.
    """

    def __init__(
        self,
        obj_id: str,
        x: float,
        y: float,
        color: Color,
        is_obstacle: bool = False,
        speed: float = 200.0,
        object_type: str = "UNKNOWN",  # Default to UNKNOWN, will be standardized to uppercase
        display_ids: (
            list[str] | None
        ) = None,  # Renamed from animation_asset_ids to display_ids
        # Client-side properties, not part of server's authoritative state
        _decay_time: float | None = None,
    ):
        """
        Initializes a new GameObject.

        Args:
            obj_id (str): A unique identifier for the object.
            x (float): The X-coordinate of the object's position in world space.
            y (float): The Y-coordinate of the object's position in world space.
            color (Color): The Raylib Color object for rendering the object.
            is_obstacle (bool): True if the object acts as an obstacle for pathfinding.
            speed (float): The movement speed of the object in pixels per second.
            object_type (str): A string representing the type of object (e.g., "PLAYER", "WALL").
            display_ids (list[str] | None): A list of display IDs for rendering this object (e.g., layers of animation).
                                                    If None, default IDs based on object_type will be used.
            _decay_time (float | None): For client-side only objects, the timestamp at which
                                        the object should be removed from the client. None for permanent objects.
        """
        self.obj_id = obj_id
        self.x = x
        self.y = y
        self.color = color
        self.is_obstacle = is_obstacle
        self.speed = speed
        self.object_type = (
            object_type.upper()
        )  # Standardize to uppercase for consistency

        # If display_ids are not provided, use the default mapping from config
        if display_ids is None:
            self.display_ids = OBJECT_TYPE_TO_DISPLAY_IDS.get(self.object_type, [])
        else:
            self.display_ids = display_ids

        self.path: list[dict[str, float]] = (
            []
        )  # List of {"X": float, "Y": float} waypoints
        self.path_index = 0  # Current index in the path

        # Store previous position for delta calculation
        self._prev_x = x
        self._prev_y = y

        # Client-side property: time after which a non-server-priority object decays
        self._decay_time = _decay_time

    @property
    def server_priority(self) -> bool:
        """
        Determines if this object's state is primarily managed by the server.
        Client-side visuals (like click pointers, path points) are not server-priority.
        """
        return SERVER_PRIORITY_OBJECT_TYPES.get(self.object_type, False)

    @property
    def decay_time(self) -> float | None:
        """
        Returns the decay time for client-side objects.
        Server-priority objects do not decay client-side.
        """
        return self._decay_time

    def set_path(self, path: list[dict[str, float]]):
        """
        Sets a new path for the object to follow.

        Args:
            path (list[dict[str, float]]): A list of waypoints.
        """
        self.path = path
        self.path_index = 0

    def update_position(self, delta_time: float) -> tuple[float, float]:
        """
        Updates the object's position based on its current path and speed.
        Calculates and returns the movement delta (dx, dy) for this frame.

        Args:
            delta_time (float): The time elapsed since the last update in seconds.

        Returns:
            tuple[float, float]: The (dx, dy) movement for this frame.
        """
        prev_x, prev_y = self.x, self.y

        if not self.path or self.path_index >= len(self.path):
            self._prev_x = self.x  # Update previous position even if not moving
            self._prev_y = self.y
            return 0.0, 0.0  # Return zero deltas if not moving

        target_point = self.path[self.path_index]
        target_world_x = target_point["X"]
        target_world_y = target_point["Y"]

        # Calculate distance to the current target point
        dx_to_target = target_world_x - self.x
        dy_to_target = target_world_y - self.y
        distance_to_target = math.sqrt(
            dx_to_target * dx_to_target + dy_to_target * dy_to_target
        )

        # Calculate how far the object can move in this frame
        move_distance = self.speed * delta_time

        if distance_to_target < move_distance:
            # If the object is very close to or has overshot the target point,
            # snap to the target point and advance to the next point in the path.
            self.x = target_world_x
            self.y = target_world_y
            self.path_index += 1
        else:
            # Move towards the target point
            # Normalize the direction vector
            direction_x = dx_to_target / distance_to_target
            direction_y = dy_to_target / distance_to_target
            self.x += direction_x * move_distance
            self.y += direction_y * move_distance

        # Calculate the actual movement delta for this frame
        current_dx = self.x - prev_x
        current_dy = self.y - prev_y

        # Update previous position for the next frame's delta calculation
        self._prev_x = self.x
        self._prev_y = self.y

        return current_dx, current_dy

    @classmethod
    def from_dict(cls, data: dict):
        """
        Creates a GameObject instance from a dictionary, typically received from the server.
        This method assumes the incoming data is authoritative from the server.

        Args:
            data (dict): A dictionary containing the object's properties as sent by the server.

        Returns:
            GameObject: A new GameObject instance.
        """
        obj_id = data["obj_id"]
        x = data["x"]
        y = data["y"]

        # Safely get color components with default values if not present
        color_r = data.get("color_r", 255)  # Default to white
        color_g = data.get("color_g", 255)
        color_b = data.get("color_b", 255)
        color_a = data.get("color_a", 255)
        color = Color(color_r, color_g, color_b, color_a)

        is_obstacle = data.get("is_obstacle", False)
        speed = data.get("speed", 200.0)
        object_type = data.get("object_type", "UNKNOWN")
        # display_ids are NOT sent by the server; the client assigns them based on object_type.
        # Pass None to __init__ to trigger the default assignment based on OBJECT_TYPE_TO_DISPLAY_IDS.
        display_ids_from_server = data.get(
            "display_ids"
        )  # Check if server explicitly sent them

        obj = cls(
            obj_id=obj_id,
            x=x,
            y=y,
            color=color,
            is_obstacle=is_obstacle,
            speed=speed,
            object_type=object_type,
            display_ids=display_ids_from_server,  # Pass what server sends, or None
            _decay_time=None,  # Server-authoritative objects don't decay client-side
        )
        if "path" in data:
            obj.set_path(data["path"])
        return obj

    def to_dict(self) -> dict:
        """
        Converts the GameObject instance to a dictionary, suitable for sending to the server.
        Only includes fields that the server expects.

        Returns:
            dict: A dictionary representation of the GameObject.
        """
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
            "object_type": self.object_type,
            "path": self.path,
            # display_ids are client-side, not sent to server typically
        }
