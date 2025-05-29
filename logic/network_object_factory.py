import logging
import random
import uuid

from raylibpy import Color

from config import (
    NETWORK_OBJECT_SIZE,
    NETWORK_OBJECT_TYPE_DEFAULT_OBJECT_LAYER_IDS,
    WORLD_HEIGHT,
    WORLD_WIDTH,
)
from object_layer.object_layer_data import ObjectLayerMode, Direction
from logic.network_object import NetworkObject

logging.basicConfig(
    level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s"
)


class NetworkObjectFactory:
    def __init__(self):
        logging.info("NetworkObjectFactory initialized.")

    def get_object_layer_data(self) -> dict:
        from object_layer.object_layer_data import OBJECT_LAYER_DATA

        return OBJECT_LAYER_DATA

    def generate_initial_state_dict(self) -> dict:
        initial_network_objects = {}

        player_id = str(uuid.uuid4())
        initial_network_objects[player_id] = NetworkObject(
            obj_id=player_id,
            x=WORLD_WIDTH / 2 - NETWORK_OBJECT_SIZE / 2,
            y=WORLD_HEIGHT / 2 - NETWORK_OBJECT_SIZE / 2,
            color=Color(0, 121, 241, 255),
            network_object_type="PLAYER",
            is_obstacle=False,
            speed=200.0,
            object_layer_ids=NETWORK_OBJECT_TYPE_DEFAULT_OBJECT_LAYER_IDS["PLAYER"],
            is_persistent=True,
        ).to_dict()

        wall_count = 5
        for i in range(wall_count):
            wall_id = str(uuid.uuid4())
            wall_x = (
                random.randint(0, WORLD_WIDTH // NETWORK_OBJECT_SIZE - 1)
                * NETWORK_OBJECT_SIZE
            )
            wall_y = (
                random.randint(0, WORLD_HEIGHT // NETWORK_OBJECT_SIZE - 1)
                * NETWORK_OBJECT_SIZE
            )
            initial_network_objects[wall_id] = NetworkObject(
                obj_id=wall_id,
                x=wall_x,
                y=wall_y,
                color=Color(100, 100, 100, 255),
                network_object_type="WALL",
                is_obstacle=True,
                object_layer_ids=NETWORK_OBJECT_TYPE_DEFAULT_OBJECT_LAYER_IDS["WALL"],
                is_persistent=True,
            ).to_dict()

        logging.info(
            f"Generated initial state with {len(initial_network_objects)} network objects."
        )
        return {
            "type": "network_state_update",
            "network_objects": initial_network_objects,
        }

    def generate_point_path(
        self, path_coords: list[dict[str, float]], current_time: float
    ) -> list[NetworkObject]:
        path_network_objects = []
        decay_duration = 2.0
        for i, point in enumerate(path_coords):
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
