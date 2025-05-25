# --- Instance Entities ---

import logging

import math


from raylibpy import (
    Color,
)

# --- Logging Configuration ---
logging.basicConfig(
    level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s"
)


class InstanceObject:
    """
    Represents a generic object within an instance world.
    This object is a local representation of the server's state.
    """

    def __init__(
        self,
        obj_id: str,
        x: float,
        y: float,
        color: Color,
        is_obstacle: bool = False,
        speed: float = 200.0,
    ):
        self.obj_id = obj_id
        self.x = x
        self.y = y
        self.color = color
        self.is_obstacle = is_obstacle
        self.speed = speed  # Pixels per second
        self.path: list[dict[str, float]] = (
            []
        )  # List of {'X': float, 'Y': float} world coordinates to follow
        self.path_index = 0

    def to_dict(self) -> dict:
        """Converts object state to a dictionary for serialization."""
        # Included for completeness, although the client does not use it to send full objects.
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
        }

    @classmethod
    def from_dict(cls, data: dict) -> "InstanceObject":
        """Creates an InstanceObject instance from a dictionary received from the server."""
        color_data = data["color"]
        color = Color(
            color_data["R"], color_data["G"], color_data["B"], color_data["A"]
        )
        obj = cls(
            data["obj_id"],
            data["x"],
            data["y"],
            color,
            data["is_obstacle"],
            data.get("speed", 200.0),  # Default speed if not provided
        )
        # Ensure path is always a list, even if Go sends null for an empty slice
        obj.path = data.get("path") if data.get("path") is not None else []
        obj.path_index = data.get("path_index", 0)
        return obj

    def update_position(self, delta_time: float):
        """
        Updates the object's position, moving it along its path.
        Movement is smoothed based on delta_time and object speed.
        This logic is for smooth animation on the client.
        """
        if not self.path or self.path_index >= len(self.path):
            self.path = []
            self.path_index = 0
            return

        target_point = self.path[self.path_index]
        target_world_x = target_point["X"]
        target_world_y = target_point["Y"]

        # Calculate distance to target
        dx = target_world_x - self.x
        dy = target_world_y - self.y
        distance = math.sqrt(dx * dx + dy * dy)

        move_distance = (
            self.speed * delta_time
        )  # Distance the object can move in this frame

        if distance < move_distance:
            # If the distance is less than what it can move, it means it has reached or overshot the point.
            # Move exactly to the point and advance to the next one in the path.
            self.x = target_world_x
            self.y = target_world_y
            self.path_index += 1
        else:
            # Move towards the target point
            direction_x = dx / distance  # X component of the normalized direction
            direction_y = dy / distance  # Y component of the normalized direction
            self.x += direction_x * move_distance
            self.y += direction_y * move_distance
