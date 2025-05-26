import logging
import math
import collections  # Still needed for other potential uses, though _direction_history is moved

from raylibpy import Color

# --- Logging Configuration ---
logging.basicConfig(
    level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s"
)

# Direction enum is no longer imported here as GameObject doesn't manage it directly
# from visuals.animation_manager import Direction

# DIRECTION_HISTORY_LENGTH is no longer imported here as GameObject doesn't manage it directly
# from config import DIRECTION_HISTORY_LENGTH


class GameObject:
    """
    Represents a generic object within an instance world.
    This object is a local representation of the server's state,
    and includes properties for position, color, obstacle status,
    speed, and pathfinding.

    Directional animation logic is now managed externally by the AnimationManager.
    """

    def __init__(
        self,
        obj_id: str,
        x: float,
        y: float,
        color: Color,
        is_obstacle: bool = False,
        speed: float = 200.0,
        object_type: str = "unknown",
        display_ids: list[str] | None = None,
    ):
        """
        Initializes a new GameObject.

        Args:
            obj_id (str): A unique identifier for the object.
            x (float): The X-coordinate of the object's position in world space.
            y (float): The Y-coordinate of the object's position in world space.
            color (Color): The Raylib Color object for rendering the object.
            is_obstacle (bool): True if the object acts as an obstacle in the world.
            speed (float): The movement speed of the object in pixels per second.
            object_type (str): The category of the object (e.g., 'player', 'wall').
            display_ids (list[str] | None): A list of animation IDs to render for this object,
                                            ordered by Z-layer (higher index = on top).
        """
        self.obj_id = obj_id
        self.x = x
        self.y = y
        self.color = color
        self.is_obstacle = is_obstacle
        self.speed = speed  # Pixels per second
        self.object_type = object_type
        self.display_ids = display_ids if display_ids is not None else []

        self.path: list[dict[str, float]] = (
            []
        )  # List of {'X': float, 'Y': float} world coordinates to follow
        self.path_index = 0

        # Track previous position for delta calculation, used by external systems
        self._prev_x = x
        self._prev_y = y

    def to_dict(self) -> dict:
        """
        Converts the GameObject's state to a dictionary for serialization.
        This is primarily for server-side use or debugging, as the client
        typically receives state updates rather than sending full object states.
        """
        return {
            "obj_id": self.obj_id,
            "x": self.x,
            "y": self.y,
            "color": {
                "R": self.color.r,
                "G": self.color.g,
                "B": self.color.b,
                "A": self.color.a,
            },
            "is_obstacle": self.is_obstacle,
            "speed": self.speed,
            "path": self.path,
            "path_index": self.path_index,
            "object_type": self.object_type,
            "display_ids": self.display_ids,
        }

    @classmethod
    def from_dict(cls, data: dict) -> "GameObject":
        """
        Creates a GameObject instance from a dictionary received from the server.

        Args:
            data (dict): A dictionary containing the object's state.

        Returns:
            GameObject: A new GameObject instance populated with the provided data.
        """
        # It should be handled by the AnimationManager or other rendering logic.

        obj = cls(
            data["obj_id"],
            data["x"],
            data["y"],
            Color(
                data["color"]["R"],
                data["color"]["G"],
                data["color"]["B"],
                data["color"]["A"],
            ),
            data["is_obstacle"],
            data.get("speed", 200.0),
            data.get("object_type", "unknown"),
            data.get("display_ids", []),
        )
        # Ensure path is always a list, even if Go sends null for an empty slice
        obj.path = data.get("path") if data.get("path") is not None else []
        obj.path_index = data.get("path_index", 0)

        # Initialize _prev_x and _prev_y to current position when loading from dict
        obj._prev_x = obj.x
        obj._prev_y = obj.y

        return obj

    def update_position(self, delta_time: float) -> tuple[float, float]:
        """
        Updates the object's position, moving it along its path.
        Movement is smoothed based on delta_time and object speed.
        This logic is for smooth interpolation on the client.

        Returns the delta X and delta Y of the object's movement in this frame.
        """
        prev_x, prev_y = self.x, self.y  # Capture position before update

        # If no path or path is completed, reset path and return zero deltas
        if not self.path or self.path_index >= len(self.path):
            self.path = []
            self.path_index = 0
            # Update _prev_x, _prev_y to current static position
            self._prev_x = self.x
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
