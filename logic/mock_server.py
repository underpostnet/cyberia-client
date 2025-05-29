import logging
import random
import time
import uuid

from raylibpy import Color

from config import (
    OBJECT_SIZE,
    OBJECT_TYPE_DEFAULT_DISPLAY_IDS,
    WORLD_HEIGHT,
    WORLD_WIDTH,
)
from logic.game_object import GameObject

logging.basicConfig(
    level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s"
)


class MockServer:
    def __init__(self):
        logging.info("MockServer initialized.")

    def generate_initial_state_dict(self) -> dict:
        initial_objects = {}

        player_id = str(uuid.uuid4())
        initial_objects[player_id] = GameObject(
            obj_id=player_id,
            x=WORLD_WIDTH / 2 - OBJECT_SIZE / 2,
            y=WORLD_HEIGHT / 2 - OBJECT_SIZE / 2,
            color=Color(0, 121, 241, 255),
            object_type="PLAYER",
            is_obstacle=False,
            speed=200.0,
            display_ids=OBJECT_TYPE_DEFAULT_DISPLAY_IDS["PLAYER"],
        ).to_dict()

        wall_count = 5
        for i in range(wall_count):
            wall_id = str(uuid.uuid4())
            wall_x = random.randint(0, WORLD_WIDTH // OBJECT_SIZE - 1) * OBJECT_SIZE
            wall_y = random.randint(0, WORLD_HEIGHT // OBJECT_SIZE - 1) * OBJECT_SIZE
            initial_objects[wall_id] = GameObject(
                obj_id=wall_id,
                x=wall_x,
                y=wall_y,
                color=Color(100, 100, 100, 255),
                object_type="WALL",
                is_obstacle=True,
                display_ids=OBJECT_TYPE_DEFAULT_DISPLAY_IDS["WALL"],
            ).to_dict()

        logging.info(f"Generated initial state with {len(initial_objects)} objects.")
        return {
            "type": "instance_state_update",
            "objects": initial_objects,
        }

    def generate_point_path(
        self, path_coords: list[dict[str, float]], current_time: float
    ) -> list[GameObject]:
        path_objects = []
        decay_duration = 2.0
        for i, point in enumerate(path_coords):
            obj_id = f"path_point_{uuid.uuid4()}"
            path_object = GameObject(
                obj_id=obj_id,
                x=point["X"],
                y=point["Y"],
                color=Color(0, 255, 0, 150),
                object_type="POINT_PATH",
                is_obstacle=False,
                speed=0.0,
                display_ids=OBJECT_TYPE_DEFAULT_DISPLAY_IDS["POINT_PATH"],
                _decay_time=current_time + decay_duration,
            )
            path_objects.append(path_object)
        return path_objects

    def generate_click_pointer(
        self, x: float, y: float, current_time: float
    ) -> GameObject:
        obj_id = f"click_pointer_{uuid.uuid4()}"
        decay_duration = 1.0
        click_pointer = GameObject(
            obj_id=obj_id,
            x=x,
            y=y,
            color=Color(255, 255, 0, 200),
            object_type="CLICK_POINTER",
            is_obstacle=False,
            speed=0.0,
            display_ids=OBJECT_TYPE_DEFAULT_DISPLAY_IDS["CLICK_POINTER"],
            _decay_time=current_time + decay_duration,
        )
        return click_pointer
