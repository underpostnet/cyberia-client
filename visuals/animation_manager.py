import time
from enum import Enum, auto
import logging

from raylibpy import (
    Color,
)  # Only import Color, other Raylib functions via RaylibManager

# Import config for global settings (if needed, currently not used directly in this file)
import config as settings

# Imported matrices and color map for character animations
# These are now expected to be in data/animations/skin/people.py
from data.animations.skin.people import (
    SKIN_PEOPLE_MATRIX_08_0,
    SKIN_PEOPLE_MATRIX_08_1,
    SKIN_PEOPLE_MATRIX_06_0,
    SKIN_PEOPLE_MATRIX_06_1,
    SKIN_PEOPLE_MATRIX_02_0,
    SKIN_PEOPLE_MATRIX_02_1,
    SKIN_PEOPLE_MATRIX_18_0,
    SKIN_PEOPLE_MATRIX_18_1,
    SKIN_PEOPLE_MATRIX_16_0,
    SKIN_PEOPLE_MATRIX_16_1,
    SKIN_PEOPLE_MATRIX_12_0,
    SKIN_PEOPLE_MATRIX_12_1,
    SKIN_PEOPLE_MAP_COLORS,
)

# Imported matrices and color map for click pointer animation
# These are now expected to be in data/animations/gfx/click_pointer.py
from data.animations.gfx.click_pointer import (
    GFX_CLICK_POINTER_MATRIX_00,
    GFX_CLICK_POINTER_MATRIX_01,
    GFX_CLICK_POINTER_MATRIX_02,
    GFX_CLICK_POINTER_MAP_COLORS,
    GFX_CLICK_POINTER_ANIMATION_SPEED,
)

# Import the new ItemRenderer for drawing animations
from visuals.item_renderer import ItemRenderer

# Import the centralized RaylibManager
from core.raylib_manager import RaylibManager

# --- Logging Configuration ---
logging.basicConfig(
    level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s"
)


# --- Enums for Animation Control ---
class Direction(Enum):
    """
    Represents the 8 cardinal and intercardinal directions for sprite animation,
    plus a NONE direction for stateless animations (e.g., UI elements).
    """

    UP = auto()
    UP_RIGHT = auto()
    RIGHT = auto()
    DOWN_RIGHT = auto()
    DOWN = auto()
    DOWN_LEFT = auto()
    LEFT = auto()
    UP_LEFT = auto()
    NONE = auto()


class AnimationMode(Enum):
    """Represents the animation state: IDLE (inactive) or WALKING (moving)."""

    IDLE = auto()
    WALKING = auto()


# --- Global Animation Data Structure ---
# This dictionary holds all animation matrices and their associated properties,
# organized by a unique display_id (e.g., "SKIN_PEOPLE", "GFX_CLICK_POINTER").
# Each display_id maps to another dictionary containing:
# - 'frames': A nested dictionary mapping a string key (e.g., "up_idle") to a list of matrices.
# - 'colors': The color map for these matrices.
# - 'frame_duration': The time in seconds each frame is displayed.
# - 'dimensions': The base width and height of the sprite matrix (e.g., 26x26).
# - 'is_stateless': True if the animation does not use directional or idle/walking states.
ANIMATION_DATA = {
    "SKIN_PEOPLE": {
        "frames": {
            # IDLE animations (using 0X prefixed matrices, where X is direction)
            # UP animations (02 / 12 from original data, now mapped to UP)
            "up_idle": [SKIN_PEOPLE_MATRIX_02_0, SKIN_PEOPLE_MATRIX_02_1],
            "up_right_idle": [
                SKIN_PEOPLE_MATRIX_06_0,
                SKIN_PEOPLE_MATRIX_06_1,
            ],  # Uses RIGHT frames for UP_RIGHT idle
            "right_idle": [SKIN_PEOPLE_MATRIX_06_0, SKIN_PEOPLE_MATRIX_06_1],
            "down_right_idle": [
                SKIN_PEOPLE_MATRIX_06_0,
                SKIN_PEOPLE_MATRIX_06_1,
            ],  # Uses RIGHT frames for DOWN_RIGHT idle
            # DOWN animations (08 / 18 from original data, now mapped to DOWN)
            "down_idle": [SKIN_PEOPLE_MATRIX_08_0, SKIN_PEOPLE_MATRIX_08_1],
            "down_left_idle": [
                SKIN_PEOPLE_MATRIX_06_0,
                SKIN_PEOPLE_MATRIX_06_1,
            ],  # Uses RIGHT frames for DOWN_LEFT idle (will be flipped)
            "left_idle": [
                SKIN_PEOPLE_MATRIX_06_0,
                SKIN_PEOPLE_MATRIX_06_1,
            ],  # Uses RIGHT frames for LEFT idle (will be flipped)
            "up_left_idle": [
                SKIN_PEOPLE_MATRIX_06_0,
                SKIN_PEOPLE_MATRIX_06_1,
            ],  # Uses RIGHT frames for UP_LEFT idle (will be flipped)
            "default_idle": [
                SKIN_PEOPLE_MATRIX_08_0,  # Default to DOWN idle
                SKIN_PEOPLE_MATRIX_08_1,
            ],
            # MOVE animations (using 1X prefixed matrices, where X is direction)
            # UP animations (02 / 12 from original data, now mapped to UP)
            "up_move": [SKIN_PEOPLE_MATRIX_12_0, SKIN_PEOPLE_MATRIX_12_1],
            "up_right_move": [
                SKIN_PEOPLE_MATRIX_16_0,
                SKIN_PEOPLE_MATRIX_16_1,
            ],  # Uses RIGHT_MOVE frames for UP_RIGHT move
            "right_move": [SKIN_PEOPLE_MATRIX_16_0, SKIN_PEOPLE_MATRIX_16_1],
            "down_right_move": [
                SKIN_PEOPLE_MATRIX_16_0,
                SKIN_PEOPLE_MATRIX_16_1,
            ],  # Uses RIGHT_MOVE frames for DOWN_RIGHT move
            # DOWN animations (08 / 18 from original data, now mapped to DOWN)
            "down_move": [SKIN_PEOPLE_MATRIX_18_0, SKIN_PEOPLE_MATRIX_18_1],
            "down_left_move": [
                SKIN_PEOPLE_MATRIX_16_0,
                SKIN_PEOPLE_MATRIX_16_1,
            ],  # Uses RIGHT_MOVE frames for DOWN_LEFT move (will be flipped)
            "left_move": [
                SKIN_PEOPLE_MATRIX_16_0,
                SKIN_PEOPLE_MATRIX_16_1,
            ],  # Uses RIGHT_MOVE frames for LEFT move (will be flipped)
            "up_left_move": [
                SKIN_PEOPLE_MATRIX_16_0,
                SKIN_PEOPLE_MATRIX_16_1,
            ],  # Uses RIGHT_MOVE frames for UP_LEFT move (will be flipped)
        },
        "colors": SKIN_PEOPLE_MAP_COLORS,
        "frame_duration": 0.3,
        "dimensions": (26, 26),  # Original content dimensions, before padding
        "is_stateless": False,  # This animation responds to direction and mode
    },
    "GFX_CLICK_POINTER": {
        "frames": {
            # For GFX_CLICK_POINTER, 'none_idle' and 'none_move' both point to the full animation sequence.
            # This ensures it always cycles, regardless of the 'is_moving' flag.
            "none_idle": [
                GFX_CLICK_POINTER_MATRIX_00,
                GFX_CLICK_POINTER_MATRIX_01,
                GFX_CLICK_POINTER_MATRIX_02,
            ],
            "none_move": [
                GFX_CLICK_POINTER_MATRIX_00,
                GFX_CLICK_POINTER_MATRIX_01,
                GFX_CLICK_POINTER_MATRIX_02,
            ],
            "default_idle": [
                GFX_CLICK_POINTER_MATRIX_00,
                GFX_CLICK_POINTER_MATRIX_01,
                GFX_CLICK_POINTER_MATRIX_02,
            ],  # Fallback
        },
        "colors": GFX_CLICK_POINTER_MAP_COLORS,
        "frame_duration": GFX_CLICK_POINTER_ANIMATION_SPEED,
        "dimensions": (5, 5),
        "is_stateless": True,  # This animation is stateless and always animates
    },
}

# Mapping from Direction Enum to string key prefix for frame lookup
DIRECTION_TO_KEY_PREFIX = {
    Direction.UP: "up",
    Direction.UP_RIGHT: "up_right",
    Direction.RIGHT: "right",
    Direction.DOWN_RIGHT: "down_right",
    Direction.DOWN: "down",
    Direction.DOWN_LEFT: "down_left",
    Direction.LEFT: "left",
    Direction.UP_LEFT: "up_left",
    Direction.NONE: "none",
}


# --- Animation Class ---
class Animation:
    """
    Manages the state and progression of a pixel art animation from a set of matrices and a color map.
    It determines which frame should be displayed based on direction, mode (idle/walking), and time.
    It does NOT handle the actual drawing/rendering; it provides the current frame data.
    """

    def __init__(
        self,
        frames_map: dict,
        color_map: list[Color],
        frame_duration: float = 0.15,
        is_stateless: bool = False,
    ):
        """
        Initializes an Animation instance.

        Args:
            frames_map (dict): A dictionary mapping animation state keys (e.g., "up_idle") to lists of frame matrices.
            color_map (list[Color]): The color map for the pixel values in the matrices.
            frame_duration (float): The duration (in seconds) each frame is displayed.
            is_stateless (bool): If True, the animation ignores direction and mode and plays continuously.
        """
        self.frames_map = frames_map
        self.color_map = color_map
        self.frame_duration = frame_duration
        self.is_stateless = is_stateless

        self.current_frame_index = 0
        self.frame_timer = 0.0
        self.last_moving_timestamp = 0.0

        # Internal state for the animation instance
        self.current_direction = Direction.DOWN  # Default direction
        self.animation_mode = AnimationMode.IDLE  # Default mode

    def set_state(self, direction: Direction, mode: AnimationMode, timestamp: float):
        """
        Sets the desired direction and animation mode for the sprite.
        If the animation is stateless, direction and mode are ignored.
        Resets frame index and timer if the state changes.

        Args:
            direction (Direction): The current direction of the animated object.
            mode (AnimationMode): The current animation mode (IDLE or WALKING).
            timestamp (float): The current time (e.g., from time.time()).
        """
        new_direction = direction
        new_mode = mode

        if self.is_stateless:
            # Stateless animations always use NONE direction and WALKING mode to ensure continuous play
            new_direction = Direction.NONE
            new_mode = AnimationMode.IDLE

        state_changed = (self.current_direction != new_direction) or (
            self.animation_mode != new_mode
        )

        self.current_direction = new_direction
        self.animation_mode = new_mode

        if state_changed:
            self.current_frame_index = 0
            self.frame_timer = 0.0
            logging.debug(
                f"Animation state changed to Direction: {self.current_direction}, Mode: {self.animation_mode.name}"
            )

        if self.animation_mode == AnimationMode.WALKING:
            self.last_moving_timestamp = timestamp

    def _get_current_animation_key(self, timestamp: float) -> str:
        """
        Determines the appropriate animation key (e.g., "up_idle", "right_move")
        based on the animation's internal state and movement status.

        Args:
            timestamp (float): The current time (e.g., from time.time()).

        Returns:
            str: The key corresponding to the current animation sequence in frames_map.
        """
        base_direction_key = DIRECTION_TO_KEY_PREFIX.get(
            self.current_direction, "default"
        )

        is_currently_moving = (self.animation_mode == AnimationMode.WALKING) or (
            timestamp - self.last_moving_timestamp < 0.5
        )

        animation_suffix = "_move" if is_currently_moving else "_idle"
        animation_key = f"{base_direction_key}{animation_suffix}"

        # Fallback logic if specific key not found
        if animation_key not in self.frames_map:
            animation_key = f"{base_direction_key}_idle"  # Try idle variant
            if animation_key not in self.frames_map:
                animation_key = "default_idle"  # Ultimate fallback

        return animation_key

    def update(self, dt: float, timestamp: float):
        """
        Updates the animation frame based on delta time and current timestamp.
        This method progresses the animation to the next frame if enough time has passed.

        Args:
            dt (float): The delta time (time elapsed since last update) in seconds.
            timestamp (float): The current time (e.g., from time.time()).
        """
        self.frame_timer += dt

        animation_key = self._get_current_animation_key(timestamp)
        frames_list = self.frames_map.get(animation_key)

        if not frames_list:
            logging.error(
                f"No frames found for key '{animation_key}' or any fallback. Using dummy frame."
            )
            frames_list = [[[0]]]  # A single 1x1 dummy frame to prevent errors

        num_frames = len(frames_list)
        if num_frames == 0:
            logging.warning(
                f"Empty frames list for key '{animation_key}'. Animation will not play."
            )
            return

        if self.frame_timer >= self.frame_duration:
            self.current_frame_index = (self.current_frame_index + 1) % num_frames
            self.frame_timer = 0.0
        # If there's only one frame, ensure index is 0 and timer is reset
        if num_frames == 1:
            self.current_frame_index = 0
            self.frame_timer = 0.0

    def get_current_frame_data(
        self, timestamp: float
    ) -> tuple[list[list[int]], list[Color], bool, int]:
        """
        Returns the data required to render the current animation frame.

        Args:
            timestamp (float): The current time (e.g., from time.time()).

        Returns:
            tuple[list[list[int]], list[Color], bool, int]:
                - The 2D integer matrix of the current frame.
                - The color map for the current animation.
                - A boolean indicating if the sprite should be flipped horizontally.
                - The current frame index (for debugging/display).
        """
        animation_key = self._get_current_animation_key(timestamp)
        frames_list = self.frames_map.get(animation_key)

        if not frames_list:
            logging.error(
                f"Get Frame Data: No frames found for key '{animation_key}' or any fallback. Returning dummy data."
            )
            return [[[0]]], self.color_map, False, 0  # Return dummy data

        current_frame_matrix = frames_list[self.current_frame_index]

        # Determine if the sprite needs to be flipped horizontally.
        flip_horizontal = False
        if not self.is_stateless and (
            self.current_direction == Direction.LEFT
            or self.current_direction == Direction.UP_LEFT
            or self.current_direction == Direction.DOWN_LEFT  # Added for consistency
        ):
            flip_horizontal = True

        return (
            current_frame_matrix,
            self.color_map,
            flip_horizontal,
            self.current_frame_index,
        )


# --- AnimationManager Class ---
class AnimationManager:
    """
    Manages and provides Animation instances for game objects.
    Acts as a factory and cache for visual components, ensuring consistency and reusability.
    It holds a reference to the ItemRenderer to facilitate drawing.
    """

    def __init__(self, raylib_manager: RaylibManager):
        """
        Initializes the AnimationManager.

        Args:
            raylib_manager (RaylibManager): The centralized Raylib manager instance.
        """
        self._active_animations: dict[str, dict] = (
            {}
        )  # Cache for active Animation instances (obj_id -> dict)
        self.item_renderer = ItemRenderer(
            raylib_manager
        )  # Initialize ItemRenderer with RaylibManager
        logging.info("AnimationManager initialized.")

    def get_animation_data(self, display_id: str) -> dict | None:
        """
        Helper method to retrieve raw animation data from the global ANIMATION_DATA.

        Args:
            display_id (str): The ID referencing the animation data.

        Returns:
            dict | None: The animation data dictionary, or None if not found.
        """
        return ANIMATION_DATA.get(display_id)

    def get_or_create_animation(
        self,
        obj_id: str,
        display_id: str,
        desired_direction: Direction,
        desired_mode: AnimationMode,
        target_display_size_pixels: int,
        timestamp: float,
    ) -> dict:
        """
        Retrieves an existing Animation instance from the cache or creates a new one.
        Returns a dictionary containing the animation instance and its current display properties.

        Args:
            obj_id (str): The unique identifier for the game object.
            display_id (str): The ID referencing the animation data in ANIMATION_DATA (e.g., "SKIN_PEOPLE").
            desired_direction (Direction): The desired direction for the animation.
            desired_mode (AnimationMode): The desired animation mode (IDLE or WALKING).
            target_display_size_pixels (int): The target pixel size for rendering this animation.
            timestamp (float): The current time (e.g., from time.time()).

        Returns:
            dict: A dictionary containing the 'animation_instance', 'target_display_size_pixels', and 'display_id'.
        """
        animation_info = self.get_animation_data(display_id)
        if not animation_info:
            raise ValueError(f"No animation data found for display_id '{display_id}'.")

        animation_instance = None
        is_recreate_needed = False

        if obj_id in self._active_animations:
            current_cached_props = self._active_animations[obj_id]
            # Check if the core animation properties (display_id or scale) have changed
            if (
                current_cached_props["display_id"] != display_id
                or current_cached_props["target_display_size_pixels"]
                != target_display_size_pixels
            ):
                is_recreate_needed = True
            else:
                animation_instance = current_cached_props["animation_instance"]
        else:
            is_recreate_needed = True

        if is_recreate_needed:
            animation_instance = Animation(
                frames_map=animation_info["frames"],
                color_map=animation_info["colors"],
                frame_duration=animation_info["frame_duration"],
                is_stateless=animation_info["is_stateless"],
            )
            self._active_animations[obj_id] = {
                "animation_instance": animation_instance,
                "target_display_size_pixels": target_display_size_pixels,
                "display_id": display_id,
            }
            logging.info(
                f"Created new animation for obj_id '{obj_id}' with display_id '{display_id}'"
            )

        # Always set the state on the retrieved or newly created instance
        # This updates its direction, mode, and last_moving_timestamp without recreating the object
        if animation_instance:  # Ensure instance is not None before calling set_state
            animation_instance.set_state(desired_direction, desired_mode, timestamp)

        return self._active_animations[obj_id]

    def remove_animation(self, obj_id: str):
        """
        Removes an Animation instance from the cache when an object is removed.

        Args:
            obj_id (str): The unique identifier of the object whose animation should be removed.
        """
        if obj_id in self._active_animations:
            del self._active_animations[obj_id]
            logging.info(f"Removed animation for obj_id '{obj_id}'.")

    def update_all_active_animations(self, delta_time: float, current_timestamp: float):
        """
        Updates the animation frame for all currently active Animation instances.

        Args:
            delta_time (float): The time elapsed since the last update in seconds.
            current_timestamp (float): The current time (e.g., from time.time()).
        """
        for obj_id, anim_properties in self._active_animations.items():
            animation_instance = anim_properties["animation_instance"]
            animation_instance.update(dt=delta_time, timestamp=current_timestamp)

    def get_animation_properties(self, obj_id: str) -> dict | None:
        """
        Retrieves the animation properties for a given object ID, including the instance itself.

        Args:
            obj_id (str): The unique identifier of the object.

        Returns:
            dict | None: A dictionary containing animation properties, or None if not found.
        """
        return self._active_animations.get(obj_id)

    def render_object_animation(
        self,
        obj_id: str,
        screen_x: float,
        screen_y: float,
        timestamp: float,
    ):
        """
        Renders the animation for a specific game object.
        This method retrieves the animation data from the managed Animation instance
        and delegates the actual drawing to the ItemRenderer.

        Args:
            obj_id (str): The unique identifier of the object to render.
            screen_x (float): The X-coordinate on the screen for rendering.
            screen_y (float): The Y-coordinate on the screen for rendering.
            timestamp (float): The current time (e.g., from time.time()).
        """
        anim_properties = self.get_animation_properties(obj_id)
        if not anim_properties:
            logging.warning(
                f"No animation found for object ID: {obj_id}. Cannot render."
            )
            return

        animation_instance = anim_properties["animation_instance"]
        target_display_size_pixels = anim_properties["target_display_size_pixels"]

        # Get the current frame data from the animation instance
        frame_matrix, color_map, flip_horizontal, current_frame_index = (
            animation_instance.get_current_frame_data(timestamp)
        )

        # Delegate rendering to the ItemRenderer
        self.item_renderer.render_animation_frame(
            frame_matrix=frame_matrix,
            color_map=color_map,
            screen_x=screen_x,
            screen_y=screen_y,
            display_size_pixels=target_display_size_pixels,
            flip_horizontal=flip_horizontal,
        )
