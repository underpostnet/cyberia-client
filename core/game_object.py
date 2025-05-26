import logging
import math

from raylibpy import Color

# --- Logging Configuration ---
logging.basicConfig(
    level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s"
)

# Import the Direction enum from animation_manager
# This replaces the local simple Direction class
from visuals.animation_manager import Direction


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
        object_type: str = "unknown",
        display_ids: list[str] | None = None,
        last_known_direction: Direction = Direction.DOWN,
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
            last_known_direction (Direction): The last direction the object was moving.
        """
        self.obj_id = obj_id
        self.x = x
        self.y = y
        self.color = color
        self.is_obstacle = is_obstacle
        self.speed = speed  # Pixels per second
        self.object_type = object_type
        self.display_ids = display_ids if display_ids is not None else []
        self.last_known_direction = last_known_direction

        self.path: list[dict[str, float]] = (
            []
        )  # List of {'X': float, 'Y': float} world coordinates to follow
        self.path_index = 0

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
            "last_known_direction": self.last_known_direction,
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
            Direction.DOWN,
        )
        # Ensure path is always a list, even if Go sends null for an empty slice
        obj.path = data.get("path") if data.get("path") is not None else []
        obj.path_index = data.get("path_index", 0)
        return obj

    def update_position(self, delta_time: float):
        """
        Updates the object's position, moving it along its path.
        Movement is smoothed based on delta_time and object speed.
        This logic is for smooth animation on the client, interpolating
        between path points received from the server.

        Args:
            delta_time (float): The time elapsed since the last frame in seconds.
        """
        # If no path or path is completed, reset path and return
        if not self.path or self.path_index >= len(self.path):
            self.path = []
            self.path_index = 0
            return

        target_point = self.path[self.path_index]
        target_world_x = target_point["X"]
        target_world_y = target_point["Y"]

        # Calculate distance to the current target point
        dx = target_world_x - self.x
        dy = target_world_y - self.y
        distance = math.sqrt(dx * dx + dy * dy)

        # Calculate how far the object can move in this frame
        move_distance = self.speed * delta_time

        if distance < move_distance:
            # If the object is very close to or has overshot the target point,
            # snap to the target point and advance to the next point in the path.
            self.x = target_world_x
            self.y = target_world_y
            self.path_index += 1
        else:
            # Move towards the target point
            # Normalize the direction vector
            direction_x = dx / distance
            direction_y = dy / distance
            self.x += direction_x * move_distance
            self.y += direction_y * move_distance

            # Update last_known_direction only when actively moving
            # and only if there's significant movement to avoid jitter from floating point inaccuracies
            if abs(dx) > 0.1 or abs(dy) > 0.1:
                # Determine the 8-directional enum based on delta X and delta Y.
                norm_dx = 0
                if dx > 0:
                    norm_dx = 1
                elif dx < 0:
                    norm_dx = -1

                norm_dy = 0
                if dy > 0:
                    norm_dy = 1
                elif dy < 0:
                    norm_dy = -1

                if norm_dy == -1:  # Up
                    if norm_dx == 0:
                        self.last_known_direction = Direction.UP
                    elif norm_dx == 1:
                        self.last_known_direction = Direction.UP_RIGHT
                    else:
                        self.last_known_direction = Direction.UP_LEFT
                elif norm_dy == 1:  # Down
                    if norm_dx == 0:
                        self.last_known_direction = Direction.DOWN
                    elif norm_dx == 1:
                        self.last_known_direction = Direction.DOWN_RIGHT
                    else:
                        self.last_known_direction = Direction.DOWN_LEFT
                else:  # Horizontal (norm_dy == 0)
                    if norm_dx == 1:
                        self.last_known_direction = Direction.RIGHT
                    else:
                        self.last_known_direction = Direction.LEFT
