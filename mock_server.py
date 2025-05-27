import time
import uuid
import logging
import math

from raylibpy import Color

from core.game_object import GameObject
from config import OBJECT_SIZE


# --- Logging Configuration ---
logging.basicConfig(
    level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s"
)

# Decay times for client-side visual effects
POINT_PATH_DECAY_TIME = 5.0
CLICK_POINTER_DECAY_TIME = 1.0


class MockServer:
    """
    Simulates a server-like entity on the client for generating various
    game objects, including both server-priority and client-side visual effects.
    This class now acts as a factory for GameObjects and can generate a full
    initial state dictionary to simulate a server's first update.
    """

    def __init__(self):
        """
        Initializes the MockServer.
        """
        # MockServer no longer holds a dict of objects; it just generates them.
        logging.info("MockServer initialized as object factory.")

    def generate_point_path(
        self, path: list[dict[str, float]], current_time: float
    ) -> list[GameObject]:
        """
        Generates a series of 'POINT_PATH' GameObjects along a given path.
        These are client-side visual-only and will decay after a set time.

        Args:
            path (list[dict[str, float]]): The path as a list of {'X': float, 'Y': float} coordinates.
            current_time (float): The current time for setting decay_time.

        Returns:
            list[GameObject]: A list of generated POINT_PATH GameObjects.
        """
        generated_objects = []
        for i, point in enumerate(path):
            obj_id = f"point_path_{uuid.uuid4().hex}"
            point_color = Color(0, 200, 255, 180)

            point_obj = GameObject(
                obj_id=obj_id,
                x=point["X"],
                y=point["Y"],
                color=point_color,
                is_obstacle=False,
                speed=0.0,
                object_type="POINT_PATH",
                display_ids=["POINT_PATH"],
                _decay_time=current_time + POINT_PATH_DECAY_TIME,
            )
            generated_objects.append(point_obj)
        logging.debug(f"Generated {len(generated_objects)} POINT_PATH objects.")
        return generated_objects

    def generate_click_pointer(
        self, x: float, y: float, current_time: float
    ) -> GameObject:
        """
        Generates a 'CLICK_POINTER' GameObject at the specified coordinates.
        This is a client-side visual-only indicator and will decay after a set time.

        Args:
            x (float): The X-coordinate in world space.
            y (float): The Y-coordinate in world space.
            current_time (float): The current time for setting decay_time.

        Returns:
            GameObject: The generated CLICK_POINTER GameObject.
        """
        obj_id = f"click_pointer_{uuid.uuid4().hex}"
        click_color = Color(255, 255, 0, 200)

        click_obj = GameObject(
            obj_id=obj_id,
            x=x,
            y=y,
            color=click_color,
            is_obstacle=False,
            speed=0.0,
            object_type="CLICK_POINTER",
            display_ids=["CLICK_POINTER"],
            _decay_time=current_time + CLICK_POINTER_DECAY_TIME,
        )
        logging.debug(f"Generated CLICK_POINTER at ({x}, {y}).")
        return click_obj

    def generate_wall(self, x: float, y: float) -> GameObject:
        """
        Generates a 'WALL' GameObject at the specified coordinates.
        This is a server-priority object (no client-side decay).

        Returns:
            GameObject: The generated WALL GameObject.
        """
        obj_id = f"wall_{uuid.uuid4().hex}"
        wall_color = Color(100, 100, 100, 255)
        wall_obj = GameObject(
            obj_id=obj_id,
            x=x,
            y=y,
            color=wall_color,
            is_obstacle=True,
            speed=0.0,
            object_type="WALL",
            display_ids=["BUILDING_WALL"],
            _decay_time=None,
        )
        logging.debug(f"Generated WALL at ({x}, {y}).")
        return wall_obj

    def generate_player(
        self, x: float, y: float, player_id: str | None = None
    ) -> GameObject:
        """
        Generates a 'PLAYER' GameObject at the specified coordinates.
        This is a server-priority object (no client-side decay).

        Args:
            player_id (str | None): Optional, specific ID for the player.

        Returns:
            GameObject: The generated PLAYER GameObject.
        """
        obj_id = player_id if player_id else f"player_{uuid.uuid4().hex}"
        player_color = Color(0, 255, 0, 255)
        player_obj = GameObject(
            obj_id=obj_id,
            x=x,
            y=y,
            color=player_color,
            is_obstacle=False,
            speed=200.0,
            object_type="PLAYER",
            display_ids=["SKIN_PEOPLE"],
            _decay_time=None,
        )
        logging.debug(f"Generated PLAYER at ({x}, {y}).")
        return player_obj

    def generate_initial_state_dict(self) -> dict:
        """
        Generates a dictionary representing an initial server state,
        including mock walls and a mock player. This is used to simulate
        the first 'instance_state_update' message from the server.

        Returns:
            dict: A dictionary structured like a server's instance_state_update.
        """
        initial_objects = {}
        # Generate mock walls
        wall1 = self.generate_wall(200, 200)
        wall2 = self.generate_wall(250, 200)
        wall3 = self.generate_wall(200, 250)
        initial_objects[wall1.obj_id] = wall1.to_dict()
        initial_objects[wall2.obj_id] = wall2.to_dict()
        initial_objects[wall3.obj_id] = wall3.to_dict()

        # Generate a mock player
        mock_player = self.generate_player(100, 100, player_id="mock_player_1")
        initial_objects[mock_player.obj_id] = mock_player.to_dict()

        return {
            "type": "instance_state_update",
            "objects": initial_objects,
        }
