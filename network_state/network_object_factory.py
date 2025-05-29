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
from network_state.network_object import NetworkObject

logging.basicConfig(
    level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s"
)


class NetworkObjectFactory:
    """
    Factory for creating various types of NetworkObjects.
    """

    def get_object_layer_data(self) -> dict:
        """Imports and returns the object layer data."""
        from object_layer.object_layer_data import OBJECT_LAYER_DATA

        return OBJECT_LAYER_DATA

    def generate_initial_state_dict(self) -> dict:
        """
        Generates an initial set of network objects for the game world,
        including a player and some walls.
        """
        initial_network_objects = {}

        # Generate player object
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

        # Generate some wall objects
        wall_count = 5
        for _ in range(wall_count):
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

        return {
            "type": "network_state_update",
            "network_objects": initial_network_objects,
        }

    def generate_point_path(
        self, path_coords: list[dict[str, float]], current_time: float
    ) -> list[NetworkObject]:
        """
        Generates a list of NetworkObjects representing points along a path.
        These are typically used for visual effects and are not persistent.
        """
        path_network_objects = []
        decay_duration = 2.0  # Path points decay after 2 seconds
        for point in path_coords:
            obj_id = f"path_point_{uuid.uuid4()}"
            path_network_object = NetworkObject(
                obj_id=obj_id,
                x=point["X"],
                y=point["Y"],
                color=Color(0, 255, 0, 150),  # Green with some transparency
                network_object_type="POINT_PATH",
                is_obstacle=False,
                speed=0.0,
                object_layer_ids=NETWORK_OBJECT_TYPE_DEFAULT_OBJECT_LAYER_IDS[
                    "POINT_PATH"
                ],
                decay_time=current_time + decay_duration,
                is_persistent=False,  # These objects are temporary
            )
            path_network_objects.append(path_network_object)
        return path_network_objects

    def generate_click_pointer(
        self, x: float, y: float, current_time: float
    ) -> NetworkObject:
        """
        Generates a NetworkObject representing a visual click pointer.
        This is temporary and decays over time.
        """
        obj_id = f"click_pointer_{uuid.uuid4()}"
        decay_duration = 1.0  # Click pointer decays after 1 second
        click_pointer = NetworkObject(
            obj_id=obj_id,
            x=x,
            y=y,
            color=Color(255, 255, 0, 200),  # Yellow with some transparency
            network_object_type="CLICK_POINTER",
            is_obstacle=False,
            speed=0.0,
            object_layer_ids=NETWORK_OBJECT_TYPE_DEFAULT_OBJECT_LAYER_IDS[
                "CLICK_POINTER"
            ],
            decay_time=current_time + decay_duration,
            is_persistent=False,  # This object is temporary
        )
        return click_pointer
