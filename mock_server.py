import uuid
import time
import random
import logging

from raylibpy import Color

from core.game_object import GameObject
from config import (
    WORLD_WIDTH,
    WORLD_HEIGHT,
    OBJECT_SIZE,
    OBJECT_TYPE_PLAYER,
    OBJECT_TYPE_WALL,
    OBJECT_TYPE_POINT_PATH,
    OBJECT_TYPE_CLICK_POINTER,
    ANIMATION_ASSET_PEOPLE,
    ANIMATION_ASSET_WALL,
    ANIMATION_ASSET_CLICK_POINTER,
    ANIMATION_ASSET_POINT_PATH,
)

# --- Logging Configuration ---
logging.basicConfig(
    level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s"
)


class MockServer:
    """
    A simplified mock server to simulate initial game state and client-side
    visual effects (like click pointers and path points).
    This class is purely for client-side demonstration and does not involve
    actual network communication.
    """

    def __init__(self):
        logging.info("MockServer initialized.")

    def generate_initial_state_dict(self) -> dict:
        """
        Generates a dictionary representing an initial instance state,
        including players and walls. This simulates what a server might send.
        """
        initial_objects = {}

        # Generate a mock player
        player_id = str(uuid.uuid4())
        initial_objects[player_id] = GameObject(
            obj_id=player_id,
            x=WORLD_WIDTH / 2 - OBJECT_SIZE / 2,
            y=WORLD_HEIGHT / 2 - OBJECT_SIZE / 2,
            color=Color(0, 121, 241, 255),  # Raylib blue
            object_type=OBJECT_TYPE_PLAYER,
            is_obstacle=False,
            speed=200.0,
            animation_asset_ids=[
                ANIMATION_ASSET_PEOPLE
            ],  # Assign default animation asset
        ).to_dict()  # Convert to dict as if from server

        # Generate some mock walls
        wall_count = 5
        for i in range(wall_count):
            wall_id = str(uuid.uuid4())
            wall_x = random.randint(0, WORLD_WIDTH // OBJECT_SIZE - 1) * OBJECT_SIZE
            wall_y = random.randint(0, WORLD_HEIGHT // OBJECT_SIZE - 1) * OBJECT_SIZE
            initial_objects[wall_id] = GameObject(
                obj_id=wall_id,
                x=wall_x,
                y=wall_y,
                color=Color(100, 100, 100, 255),  # Gray
                object_type=OBJECT_TYPE_WALL,
                is_obstacle=True,
                animation_asset_ids=[
                    ANIMATION_ASSET_WALL
                ],  # Assign default animation asset
            ).to_dict()  # Convert to dict as if from server

        logging.info(f"Generated initial state with {len(initial_objects)} objects.")
        return {
            "type": "instance_state_update",
            "objects": initial_objects,
        }

    def generate_point_path(
        self, path_coords: list[dict[str, float]], current_time: float
    ) -> list[GameObject]:
        """
        Generates client-side GameObject instances for path visualization.
        These objects are not server-authoritative and will decay.
        """
        path_objects = []
        decay_duration = 2.0  # Path points disappear after 2 seconds
        for i, point in enumerate(path_coords):
            obj_id = f"path_point_{uuid.uuid4()}"  # Unique ID for each path point
            path_object = GameObject(
                obj_id=obj_id,
                x=point["X"],
                y=point["Y"],
                color=Color(0, 255, 0, 150),  # Semi-transparent green
                object_type=OBJECT_TYPE_POINT_PATH,
                is_obstacle=False,
                speed=0.0,  # Path points don't move
                animation_asset_ids=[
                    ANIMATION_ASSET_POINT_PATH
                ],  # Assign default animation asset
                _decay_time=current_time + decay_duration,
            )
            path_objects.append(path_object)
        return path_objects

    def generate_click_pointer(
        self, x: float, y: float, current_time: float
    ) -> GameObject:
        """
        Generates a client-side GameObject instance for a click pointer.
        This object is not server-authoritative and will decay.
        """
        obj_id = f"click_pointer_{uuid.uuid4()}"
        decay_duration = 1.0  # Click pointer disappears after 1 second
        click_pointer = GameObject(
            obj_id=obj_id,
            x=x,
            y=y,
            color=Color(255, 255, 0, 200),  # Semi-transparent yellow
            object_type=OBJECT_TYPE_CLICK_POINTER,
            is_obstacle=False,
            speed=0.0,  # Click pointer doesn't move
            animation_asset_ids=[
                ANIMATION_ASSET_CLICK_POINTER
            ],  # Assign default animation asset
            _decay_time=current_time + decay_duration,
        )
        return click_pointer
