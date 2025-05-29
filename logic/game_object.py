import logging
import math

from raylibpy import Color

from config import OBJECT_TYPE_DEFAULT_DISPLAY_IDS, SERVER_PRIORITY_OBJECT_TYPES

logging.basicConfig(
    level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s"
)


class GameObject:
    def __init__(
        self,
        obj_id: str,
        x: float,
        y: float,
        color: Color,
        is_obstacle: bool = False,
        speed: float = 200.0,
        object_type: str = "UNKNOWN",
        display_ids: list[str] | None = None,
        _decay_time: float | None = None,
    ):
        self.obj_id = obj_id
        self.x = x
        self.y = y
        self.color = color
        self.is_obstacle = is_obstacle
        self.speed = speed
        self.object_type = object_type.upper()

        if display_ids is None:
            self.display_ids = OBJECT_TYPE_DEFAULT_DISPLAY_IDS.get(self.object_type, [])
        else:
            self.display_ids = display_ids

        self.path: list[dict[str, float]] = []
        self.path_index = 0

        self._prev_x = x
        self._prev_y = y

        self._decay_time = _decay_time

    @property
    def server_priority(self) -> bool:
        return SERVER_PRIORITY_OBJECT_TYPES.get(self.object_type, False)

    @property
    def decay_time(self) -> float | None:
        return self._decay_time

    def set_path(self, path: list[dict[str, float]]):
        self.path = path
        self.path_index = 0

    def update_position(self, delta_time: float) -> tuple[float, float]:
        prev_x, prev_y = self.x, self.y

        if not self.path or self.path_index >= len(self.path):
            self._prev_x = self.x
            self._prev_y = self.y
            return 0.0, 0.0

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
            self.x = target_world_x
            self.y = target_world_y
            self.path_index += 1
        else:
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
        object_type = data.get("object_type", "UNKNOWN")
        display_ids_from_server = data.get("display_ids")

        obj = cls(
            obj_id=obj_id,
            x=x,
            y=y,
            color=color,
            is_obstacle=is_obstacle,
            speed=speed,
            object_type=object_type,
            display_ids=display_ids_from_server,
            _decay_time=None,
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
            "object_type": self.object_type,
            "path": self.path,
        }
