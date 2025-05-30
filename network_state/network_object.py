import logging
import math

from raylibpy import Color

from config import (
    NETWORK_OBJECT_TYPE_DEFAULT_OBJECT_LAYER_IDS,
    WORLD_WIDTH,
    WORLD_HEIGHT,
)

logging.basicConfig(
    level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s"
)


class NetworkObject:
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

        self.object_layer_ids = (
            object_layer_ids
            if object_layer_ids is not None
            else NETWORK_OBJECT_TYPE_DEFAULT_OBJECT_LAYER_IDS.get(
                self.network_object_type, []
            )
        )

        self.path: list[dict[str, float]] = []
        self.path_index: int = 0

        self.decay_time = decay_time
        self.is_persistent = is_persistent

    def set_path(self, path: list[dict[str, float]]):
        self.path = path
        self.path_index = 0

    def update_position(self, delta_time: float) -> tuple[float, float]:
        """
        Updates the object's position along its path.
        Returns the effective delta_x and delta_y considering world wrapping.
        """
        if not self.path or self.path_index >= len(self.path):
            return 0.0, 0.0

        target_point = self.path[self.path_index]
        target_x = target_point["X"]
        target_y = target_point["Y"]

        # Store old position before update
        old_x = self.x
        old_y = self.y

        # Calculate vector to target, considering world wrapping for shortest path
        # This is for movement calculation, not visual rendering
        diff_x = target_x - self.x
        diff_y = target_y - self.y

        # Adjust diff_x for wrapping
        if abs(diff_x) > WORLD_WIDTH / 2:
            if diff_x > 0:
                diff_x -= WORLD_WIDTH
            else:
                diff_x += WORLD_WIDTH

        # Adjust diff_y for wrapping
        if abs(diff_y) > WORLD_HEIGHT / 2:
            if diff_y > 0:
                diff_y -= WORLD_HEIGHT
            else:
                diff_y += WORLD_HEIGHT

        distance = math.sqrt(diff_x**2 + diff_y**2)

        if distance < 1.0:  # Close enough to target, move to next point
            self.x = target_x % WORLD_WIDTH
            self.y = target_y % WORLD_HEIGHT
            if self.x < 0:
                self.x += WORLD_WIDTH
            if self.y < 0:
                self.y += WORLD_HEIGHT
            self.path_index += 1
            return diff_x, diff_y  # Return the delta for the last step to target
        else:
            move_distance = min(self.speed * delta_time, distance)
            move_x = (diff_x / distance) * move_distance
            move_y = (diff_y / distance) * move_distance

            self.x = (self.x + move_x) % WORLD_WIDTH
            self.y = (self.y + move_y) % WORLD_HEIGHT

            # Ensure coordinates are positive after modulo
            if self.x < 0:
                self.x += WORLD_WIDTH
            if self.y < 0:
                self.y += WORLD_HEIGHT

            # Calculate effective_dx and effective_dy for rendering direction
            effective_dx = self.x - old_x
            effective_dy = self.y - old_y

            # Adjust effective_dx/dy if a wrap occurred during this step
            if abs(effective_dx) > WORLD_WIDTH / 2:
                if effective_dx > 0:
                    effective_dx -= WORLD_WIDTH
                else:
                    effective_dx += WORLD_WIDTH

            if abs(effective_dy) > WORLD_HEIGHT / 2:
                if effective_dy > 0:
                    effective_dy -= WORLD_HEIGHT
                else:
                    effective_dy += WORLD_HEIGHT

            return effective_dx, effective_dy

    @classmethod
    def from_dict(cls, data: dict) -> "NetworkObject":
        obj_id = data["obj_id"]
        x = data["x"]
        y = data["y"]

        # --- START MODIFICATION FOR COLOR PARSING ---
        if (
            "color_r" in data
            and "color_g" in data
            and "color_b" in data
            and "color_a" in data
        ):
            # New flattened format from Go server
            color_r = data["color_r"]
            color_g = data["color_g"]
            color_b = data["color_b"]
            color_a = data["color_a"]
        elif "color" in data and isinstance(data["color"], dict):
            # Old nested format (fallback for older server versions or inconsistent data)
            nested_color = data["color"]
            color_r = nested_color.get("R", 0)
            color_g = nested_color.get("G", 0)
            color_b = nested_color.get("B", 0)
            color_a = nested_color.get("A", 255)  # Default alpha to 255 if not present
        else:
            # Default to black if color data is completely missing or malformed
            logging.warning(
                f"Color data missing or malformed for object {obj_id}. Defaulting to black."
            )
            color_r, color_g, color_b, color_a = 0, 0, 0, 255
        # --- END MODIFICATION FOR COLOR PARSING ---

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
            "decay_time": self.decay_time,
            "is_persistent": self.is_persistent,
            "path": self.path,
            "path_index": self.path_index,
        }
