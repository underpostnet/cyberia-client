import time
from enum import Enum, auto
import logging
import collections
import math
from raylibpy import (
    Color,
    init_window,
    set_target_fps,
    begin_drawing,
    clear_background,
    end_drawing,
    window_should_close,
    close_window,
    get_frame_time,
    draw_rectangle,
    draw_circle,
    draw_line,
    draw_text,
    begin_mode2d,
    end_mode2d,
    get_mouse_position,
    get_screen_to_world2d,
    is_mouse_button_pressed,
    is_key_pressed,
    Vector2,
    Camera2D,
    BLACK,
    LIGHTGRAY,
    GREEN,
    RED,
)

# Import config for global settings
import config as settings
from config import DIRECTION_HISTORY_LENGTH, OBJECT_SIZE

# Imported matrices and color map for character animations
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
from data.animations.gfx.click_pointer import (
    GFX_CLICK_POINTER_MATRIX_00,
    GFX_CLICK_POINTER_MATRIX_01,
    GFX_CLICK_POINTER_MATRIX_02,
    GFX_CLICK_POINTER_MAP_COLORS,
    GFX_CLICK_POINTER_ANIMATION_SPEED,
)

# Imported matrices and color map for point path animation
from data.animations.gfx.point_path import (
    GFX_POINT_PATH_MATRIX_00,
    GFX_POINT_PATH_MAP_COLORS,
    GFX_POINT_PATH_ANIMATION_SPEED,
)

# New: Imported matrices and color map for wall animation
from data.animations.building.wall import (
    BUILDING_WALL_MATRIX_00,
    BUILDING_WALL_MAP_COLORS,
    BUILDING_WALL_ANIMATION_SPEED,
)


# Import GameObject for type hinting
from core.game_object import GameObject


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
# organized by a unique display_id (e.g., "SKIN_PEOPLE", "CLICK_POINTER").
ANIMATION_DATA = {
    "SKIN_PEOPLE": {
        "frames": {
            "up_idle": [SKIN_PEOPLE_MATRIX_02_0, SKIN_PEOPLE_MATRIX_02_1],
            "up_right_idle": [SKIN_PEOPLE_MATRIX_06_0, SKIN_PEOPLE_MATRIX_06_1],
            "right_idle": [SKIN_PEOPLE_MATRIX_06_0, SKIN_PEOPLE_MATRIX_06_1],
            "down_right_idle": [SKIN_PEOPLE_MATRIX_06_0, SKIN_PEOPLE_MATRIX_06_1],
            "down_idle": [SKIN_PEOPLE_MATRIX_08_0, SKIN_PEOPLE_MATRIX_08_1],
            "down_left_idle": [SKIN_PEOPLE_MATRIX_06_0, SKIN_PEOPLE_MATRIX_06_1],
            "left_idle": [
                SKIN_PEOPLE_MATRIX_06_0,
                SKIN_PEOPLE_MATRIX_06_1,
            ],  # Corrected typo here
            "up_left_idle": [SKIN_PEOPLE_MATRIX_06_0, SKIN_PEOPLE_MATRIX_06_1],
            "default_idle": [SKIN_PEOPLE_MATRIX_08_0, SKIN_PEOPLE_MATRIX_08_1],
            "up_move": [SKIN_PEOPLE_MATRIX_12_0, SKIN_PEOPLE_MATRIX_12_1],
            "up_right_move": [SKIN_PEOPLE_MATRIX_16_0, SKIN_PEOPLE_MATRIX_16_1],
            "right_move": [SKIN_PEOPLE_MATRIX_16_0, SKIN_PEOPLE_MATRIX_16_1],
            "down_right_move": [SKIN_PEOPLE_MATRIX_16_0, SKIN_PEOPLE_MATRIX_16_1],
            "down_move": [SKIN_PEOPLE_MATRIX_18_0, SKIN_PEOPLE_MATRIX_18_1],
            "down_left_move": [SKIN_PEOPLE_MATRIX_16_0, SKIN_PEOPLE_MATRIX_16_1],
            "left_move": [SKIN_PEOPLE_MATRIX_16_0, SKIN_PEOPLE_MATRIX_16_1],
            "up_left_move": [SKIN_PEOPLE_MATRIX_16_0, SKIN_PEOPLE_MATRIX_16_1],
        },
        "colors": SKIN_PEOPLE_MAP_COLORS,
        "frame_duration": 0.3,
        "dimensions": (26, 26),  # Original content dimensions, before padding
        "is_stateless": False,
    },
    "CLICK_POINTER": {
        "frames": {
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
            ],
        },
        "colors": GFX_CLICK_POINTER_MAP_COLORS,
        "frame_duration": GFX_CLICK_POINTER_ANIMATION_SPEED,
        "dimensions": (5, 5),
        "is_stateless": True,
    },
    "POINT_PATH": {
        "frames": {
            "none_idle": [GFX_POINT_PATH_MATRIX_00],
            "none_move": [GFX_POINT_PATH_MATRIX_00],
            "default_idle": [GFX_POINT_PATH_MATRIX_00],
        },
        "colors": GFX_POINT_PATH_MAP_COLORS,
        "frame_duration": GFX_POINT_PATH_ANIMATION_SPEED,
        "dimensions": (3, 3),
        "is_stateless": True,
    },
    "BUILDING_WALL": {
        "frames": {
            "none_idle": [BUILDING_WALL_MATRIX_00],
            "none_move": [BUILDING_WALL_MATRIX_00],
            "default_idle": [BUILDING_WALL_MATRIX_00],
        },
        "colors": [Color(0, 0, 0, 0), Color(0, 0, 0, 255)],  # Transparent, Black
        "frame_duration": BUILDING_WALL_ANIMATION_SPEED,
        "dimensions": (5, 5),
        "is_stateless": True,
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

# --- Object Type Rendering Properties ---
# This dictionary defines default display_ids, base sprite scaling,
# and rendering offsets for different object types.
OBJECT_RENDER_PROPERTIES = {
    "PLAYER": {
        "default_display_ids": ["SKIN_PEOPLE"],
        "base_sprite_scale": 2,  # Example: 2x scale for player sprites
        "center_offset_x": -12,  # Adjust to visually center the sprite within OBJECT_SIZE
        "center_offset_y": -12,
    },
    "POINT_PATH": {
        "default_display_ids": ["POINT_PATH"],
        "base_sprite_scale": 1,
        "center_offset_x": -1,  # Adjust to visually center the sprite within OBJECT_SIZE
        "center_offset_y": -1,
    },
    "CLICK_POINTER": {
        "default_display_ids": ["CLICK_POINTER"],
        "base_sprite_scale": 1,
        "center_offset_x": -2,  # Adjust to visually center the sprite within OBJECT_SIZE
        "center_offset_y": -2,
    },
    "WALL": {
        "default_display_ids": ["BUILDING_WALL"],
        "base_sprite_scale": 1,
        "center_offset_x": 0,
        "center_offset_y": 0,
    },
    # Add other object types as needed with their specific rendering properties
    "UNKNOWN": {  # Default properties for unknown object types
        "default_display_ids": [],  # No animation by default
        "base_sprite_scale": 1,
        "center_offset_x": 0,
        "center_offset_y": 0,
    },
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

        if self.is_stateless:
            return "none_idle" if "none_idle" in self.frames_map else "default_idle"

        is_currently_moving = self.animation_mode == AnimationMode.WALKING
        animation_suffix = "_move" if is_currently_moving else "_idle"
        animation_key = f"{base_direction_key}{animation_suffix}"

        if animation_key not in self.frames_map:
            animation_key = f"{base_direction_key}_idle"
            if animation_key not in self.frames_map:
                animation_key = "default_idle"

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
            frames_list = [[[0]]]

        num_frames = len(frames_list)
        if num_frames == 0:
            logging.warning(
                f"Empty frames list for key '{animation_key}'. Animation will not play."
            )
            return

        if self.frame_timer >= self.frame_duration:
            self.current_frame_index = (self.current_frame_index + 1) % num_frames
            self.frame_timer = 0.0
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
            return [[[0]]], self.color_map, False, 0

        current_frame_matrix = frames_list[self.current_frame_index]

        flip_horizontal = False
        if not self.is_stateless and (
            self.current_direction == Direction.LEFT
            or self.current_direction == Direction.UP_LEFT
            or self.current_direction == Direction.DOWN_LEFT
        ):
            flip_horizontal = True

        return (
            current_frame_matrix,
            self.color_map,
            flip_horizontal,
            self.current_frame_index,
        )


class RenderingSystem:
    """
    Centralized manager for all Raylib-py interactions, game world rendering,
    and animation management. This class encapsulates window management,
    drawing primitives, input handling, camera control, grid drawing,
    game object rendering, and animation state management.
    """

    def __init__(
        self,
        screen_width: int,
        screen_height: int,
        world_width: int,
        world_height: int,
        object_size: int,
        title: str = "Raylib Application",
        target_fps: int = 60,
    ):
        """
        Initializes the RenderingSystem, including the Raylib window, camera,
        and internal animation management.

        Args:
            screen_width (int): The width of the application window.
            screen_height (int): The height of the application window.
            world_width (int): The width of the game world.
            world_height (int): The height of the game world.
            object_size (int): The size of game objects in pixels.
            title (str): The title of the application window.
            target_fps (int): The desired frames per second.
        """
        self.screen_width = screen_width
        self.screen_height = screen_height
        self.world_width = world_width
        self.world_height = world_height
        self.object_size = object_size
        self.title = title
        self.target_fps = target_fps
        init_window(self.screen_width, self.screen_height, self.title)
        set_target_fps(self.target_fps)
        logging.info(
            f"Raylib window initialized: {self.screen_width}x{self.screen_height} @ {self.target_fps} FPS"
        )

        self.camera = Camera2D()
        self.camera.offset = Vector2(self.screen_width / 2, self.screen_height / 2)
        self.camera.rotation = 0.0
        self.camera.zoom = 1.0
        self.camera.target = Vector2(0, 0)

        self._active_animations: dict[str, dict] = {}
        self._object_direction_histories: dict[
            tuple[str, str], collections.deque[Direction]
        ] = {}
        logging.info("RenderingSystem initialized.")

    def begin_drawing(self):
        """Starts the drawing phase for a new frame."""
        begin_drawing()

    def end_drawing(self):
        """Ends the drawing phase for the current frame."""
        end_drawing()

    def clear_background(self, color: Color):
        """Clears the background of the screen with a specified color."""
        clear_background(color)

    def begin_camera_mode(self):
        """Begins 2D camera mode, applying camera transformations."""
        begin_mode2d(self.camera)

    def end_camera_mode(self):
        """Ends 2D camera mode, reverting to screen space."""
        end_mode2d()

    def draw_rectangle(self, x: int, y: int, width: int, height: int, color: Color):
        """Draws a filled rectangle."""
        draw_rectangle(x, y, width, height, color)

    def draw_circle(self, center_x: int, center_y: int, radius: float, color: Color):
        """Draws a filled circle."""
        draw_circle(center_x, center_y, radius, color)

    def draw_line(
        self, start_x: int, start_y: int, end_x: int, end_y: int, color: Color
    ):
        """Draws a line between two points."""
        draw_line(start_x, start_y, end_x, end_y, color)

    def draw_text(self, text: str, x: int, y: int, font_size: int, color: Color):
        """Draws text on the screen."""
        draw_text(text, x, y, font_size, color)

    def get_frame_time(self) -> float:
        """Returns the time elapsed since the last frame in seconds."""
        return get_frame_time()

    def get_mouse_position(self) -> Vector2:
        """Returns the current mouse position in screen coordinates."""
        return get_mouse_position()

    def get_world_mouse_position(self) -> Vector2:
        """Converts current mouse screen position to world position using the active camera."""
        return get_screen_to_world2d(get_mouse_position(), self.camera)

    def is_mouse_button_pressed(self, button: int) -> bool:
        """Checks if a mouse button has been pressed in the current frame."""
        return is_mouse_button_pressed(button)

    def is_key_pressed(self, key: int) -> bool:
        """Checks if a keyboard key has been pressed in the current frame."""
        return is_key_pressed(key)

    def window_should_close(self) -> bool:
        """Checks if the window close button has been pressed or ESC key has been pressed."""
        return window_should_close()

    def close_window(self):
        """Closes the Raylib window."""
        close_window()
        logging.info("Raylib window closed.")

    def update_camera_target(self, target_world_pos: Vector2, smoothness: float = 1.0):
        """
        Smoothly moves the camera's target towards a desired world position.

        Args:
            target_world_pos (Vector2): The target world coordinates for the camera.
            smoothness (float): A value between 0.0 and 1.0 controlling camera smoothness.
                                1.0 means instant movement, 0.0 means no movement.
        """
        self.camera.target.x += (target_world_pos.x - self.camera.target.x) * smoothness
        self.camera.target.y += (target_world_pos.y - self.camera.target.y) * smoothness

    def set_camera_zoom(self, zoom_factor: float):
        """Sets the camera's zoom level."""
        self.camera.zoom = zoom_factor

    def get_camera_zoom(self) -> float:
        """Returns the current camera zoom level."""
        return self.camera.zoom

    def draw_grid(self):
        """Draws the grid lines across the game world."""
        for x in range(0, self.world_width + 1, self.object_size):
            self.draw_line(x, 0, x, self.world_height, LIGHTGRAY)
        for y in range(0, self.world_height + 1, self.object_size):
            self.draw_line(0, y, self.world_width, y, LIGHTGRAY)

    def draw_game_object(
        self,
        game_object: GameObject,
        current_timestamp: float,
        current_dx: float,
        current_dy: float,
    ):
        """
        Draws a game object, handling different object types and animation layers.

        Args:
            game_object (GameObject): The game object to draw.
            current_timestamp (float): The current time (e.g., from time.time()) for animation updates.
            current_dx (float): The delta X movement of the object in the current frame.
            current_dy (float): The delta Y movement of the object in the current frame.
        """
        render_props = OBJECT_RENDER_PROPERTIES.get(
            game_object.object_type, OBJECT_RENDER_PROPERTIES["UNKNOWN"]
        )

        display_ids_to_use = game_object.display_ids
        if not display_ids_to_use:
            display_ids_to_use = render_props["default_display_ids"]

        if not display_ids_to_use:
            # If no display_ids are found, draw a simple colored rectangle
            self.draw_rectangle(
                int(game_object.x),
                int(game_object.y),
                self.object_size,
                self.object_size,
                BLACK,
            )
            self.draw_rectangle(
                int(game_object.x) + 2,
                int(game_object.y) + 2,
                self.object_size - 4,
                self.object_size - 4,
                game_object.color,
            )
            return

        # Determine animation mode (IDLE or WALKING)
        animation_mode = AnimationMode.IDLE
        if game_object.path and game_object.path_index < len(game_object.path):
            animation_mode = AnimationMode.WALKING

        for display_id in display_ids_to_use:
            animation_info = self.get_animation_data(display_id)
            if not animation_info:
                logging.warning(
                    f"No animation info found for display_id: {display_id}. Skipping rendering for {game_object.obj_id}."
                )
                continue

            anim_base_dimensions = animation_info["dimensions"]
            base_sprite_scale = render_props["base_sprite_scale"]
            center_offset_x = render_props["center_offset_x"]
            center_offset_y = render_props["center_offset_y"]

            # Calculate target display size based on OBJECT_SIZE and sprite dimensions
            # The goal is to make the sprite fit within or scale relative to the OBJECT_SIZE grid cell
            target_display_size_pixels = int(
                base_sprite_scale * (self.object_size / anim_base_dimensions[0])
            )
            if target_display_size_pixels == 0:
                target_display_size_pixels = 1

            # Get or create the animation instance
            self.get_or_create_animation(
                obj_id=game_object.obj_id,
                display_id=display_id,
                target_display_size_pixels=target_display_size_pixels,
                initial_direction=Direction.DOWN,
            )

            # Update animation direction and mode only for non-stateless animations
            if not animation_info["is_stateless"]:
                self.update_animation_direction_for_object(
                    obj_id=game_object.obj_id,
                    display_id=display_id,
                    current_dx=current_dx,
                    current_dy=current_dy,
                    animation_mode=animation_mode,
                    timestamp=current_timestamp,
                )
            else:
                # For stateless animations, ensure their internal state is consistent
                anim_properties = self.get_animation_properties(
                    game_object.obj_id, display_id
                )
                if anim_properties:
                    animation_instance = anim_properties["animation_instance"]
                    animation_instance.set_state(
                        Direction.NONE, AnimationMode.IDLE, current_timestamp
                    )

            # Calculate drawing position, applying offsets
            draw_x = game_object.x + center_offset_x
            draw_y = game_object.y + center_offset_y

            self.render_object_animation(
                obj_id=game_object.obj_id,
                display_id=display_id,
                screen_x=draw_x,
                screen_y=draw_y,
                timestamp=current_timestamp,
            )

    def _render_animation_frame(
        self,
        frame_matrix: list[list[int]],
        color_map: list[Color],
        screen_x: float,
        screen_y: float,
        display_size_pixels: int,
        flip_horizontal: bool = False,
    ):
        """
        Renders a single frame of an animation (a pixel art matrix) at the specified
        screen coordinates with scaling. This is the core drawing logic for pixel art.

        Args:
            frame_matrix (list[list[int]]): The 2D integer matrix representing the current animation frame.
            color_map (list[Color]): A list of Raylib Color objects, where indices correspond to matrix values.
            screen_x (float): The X-coordinate on the screen where the top-left corner of the animation should be drawn.
            screen_y (float): The Y-coordinate on the screen where the top-left corner of the animation should be drawn.
            display_size_pixels (int): The desired total width and height of the rendered animation in pixels.
            flip_horizontal (bool): If True, the sprite will be drawn flipped horizontally.
        """
        matrix_dimension = len(frame_matrix)
        if matrix_dimension == 0:
            logging.warning("Render: Empty matrix provided. Cannot draw.")
            return

        rounded_screen_x = round(screen_x)
        rounded_screen_y = round(screen_y)

        for row_idx in range(matrix_dimension):
            for col_idx in range(matrix_dimension):
                matrix_value = frame_matrix[row_idx][col_idx]

                if matrix_value == 0 and len(color_map) > 0 and color_map[0].a == 0:
                    continue

                draw_col = col_idx
                if flip_horizontal:
                    draw_col = matrix_dimension - 1 - col_idx

                cell_draw_x = rounded_screen_x + draw_col * display_size_pixels
                cell_draw_y = rounded_screen_y + row_idx * display_size_pixels

                self.draw_rectangle(
                    int(cell_draw_x),
                    int(cell_draw_y),
                    display_size_pixels,
                    display_size_pixels,
                    color_map[matrix_value],
                )

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
        target_display_size_pixels: int,
        initial_direction: Direction,
    ) -> dict:
        """
        Retrieves an existing Animation instance from the cache or creates a new one.
        Returns a dictionary containing the animation instance and its current display properties.

        Args:
            obj_id (str): The unique identifier for the game object.
            display_id (str): The ID referencing the animation data in ANIMATION_DATA.
            target_display_size_pixels (int): The target pixel size for rendering this animation.
            initial_direction (Direction): The initial direction to set for the animation history.

        Returns:
            dict: A dictionary containing the 'animation_instance', 'target_display_size_pixels', and 'display_id'.
        """
        anim_key = f"{obj_id}_{display_id}"
        current_cached_props = self._active_animations.get(anim_key)

        if current_cached_props:
            if (
                current_cached_props["display_id"] == display_id
                and current_cached_props["target_display_size_pixels"]
                != target_display_size_pixels
            ):
                current_cached_props["target_display_size_pixels"] = (
                    target_display_size_pixels
                )
                logging.debug(
                    f"Updated display size for existing animation '{anim_key}'."
                )
                return current_cached_props
            elif (
                current_cached_props["display_id"] == display_id
                and current_cached_props["target_display_size_pixels"]
                == target_display_size_pixels
            ):
                return current_cached_props

        animation_info = self.get_animation_data(display_id)
        if not animation_info:
            raise ValueError(f"No animation data found for display_id '{display_id}'.")

        animation_instance = Animation(
            frames_map=animation_info["frames"],
            color_map=animation_info["colors"],
            frame_duration=animation_info["frame_duration"],
            is_stateless=animation_info["is_stateless"],
        )
        self._active_animations[anim_key] = {
            "animation_instance": animation_instance,
            "target_display_size_pixels": target_display_size_pixels,
            "display_id": display_id,
        }
        logging.info(
            f"Created new animation for obj_id '{obj_id}' with display_id '{display_id}'"
        )

        if not animation_info["is_stateless"]:
            if (obj_id, display_id) not in self._object_direction_histories:
                self._object_direction_histories[(obj_id, display_id)] = (
                    collections.deque(maxlen=DIRECTION_HISTORY_LENGTH)
                )
                self._object_direction_histories[(obj_id, display_id)].append(
                    initial_direction
                )
            else:
                if not self._object_direction_histories[(obj_id, display_id)]:
                    self._object_direction_histories[(obj_id, display_id)].append(
                        initial_direction
                    )
        else:
            if (obj_id, display_id) in self._object_direction_histories:
                del self._object_direction_histories[(obj_id, display_id)]

        return self._active_animations[anim_key]

    def remove_animation(self, obj_id: str, display_id: str | None = None):
        """
        Removes Animation instances and their associated direction histories from the cache.
        If display_id is None, removes all animations for the given obj_id.

        Args:
            obj_id (str): The unique identifier of the object whose animation should be removed.
            display_id (str | None): The specific display_id to remove, or None to remove all for obj_id.
        """
        keys_to_remove = []
        if display_id:
            anim_key = f"{obj_id}_{display_id}"
            if anim_key in self._active_animations:
                keys_to_remove.append((anim_key, (obj_id, display_id)))
        else:
            for key in list(self._active_animations.keys()):
                if key.startswith(f"{obj_id}_"):
                    parts = key.split("_", 1)
                    if len(parts) > 1:
                        keys_to_remove.append((key, (obj_id, parts[1])))
                    else:
                        keys_to_remove.append((key, (obj_id, "")))

        for anim_key, history_key in keys_to_remove:
            if anim_key in self._active_animations:
                del self._active_animations[anim_key]
                logging.info(f"Removed animation instance for key '{anim_key}'.")
            if history_key in self._object_direction_histories:
                del self._object_direction_histories[history_key]
                logging.info(f"Removed direction history for key '{history_key}'.")

    def update_animation_direction_for_object(
        self,
        obj_id: str,
        display_id: str,
        current_dx: float,
        current_dy: float,
        animation_mode: AnimationMode,
        timestamp: float,
    ):
        """
        Updates the direction history for a specific object's animation
        and sets the current direction on its Animation instance.

        Args:
            obj_id (str): The unique identifier of the object.
            display_id (str): The ID referencing the animation data.
            current_dx (float): The delta X movement of the object in the current frame.
            current_dy (float): The delta Y movement of the object in the current frame.
            animation_mode (AnimationMode): The current animation mode (IDLE or WALKING).
            timestamp (float): The current time (e.g., from time.time()).
        """
        history_key = (obj_id, display_id)
        direction_history = self._object_direction_histories.get(history_key)

        if not direction_history:
            logging.debug(
                f"Direction history not found for {history_key}. Skipping direction update."
            )
            return

        movement_threshold = 1.0
        current_movement_magnitude = math.sqrt(current_dx**2 + current_dy**2)

        if current_movement_magnitude > movement_threshold:
            norm_dx = 0
            if current_dx > 0:
                norm_dx = 1
            elif current_dx < 0:
                norm_dx = -1

            norm_dy = 0
            if current_dy > 0:
                norm_dy = 1
            elif current_dy < 0:
                norm_dy = -1

            direction_map = {
                (0, -1): Direction.UP,
                (1, -1): Direction.UP_RIGHT,
                (1, 0): Direction.RIGHT,
                (1, 1): Direction.DOWN_RIGHT,
                (0, 1): Direction.DOWN,
                (-1, 1): Direction.DOWN_LEFT,
                (-1, 0): Direction.LEFT,
                (-1, -1): Direction.UP_LEFT,
            }
            instantaneous_direction = direction_map.get(
                (norm_dx, norm_dy), Direction.DOWN
            )

            direction_history.append(instantaneous_direction)
        else:
            if len(direction_history) > 1:
                direction_history.popleft()

        smoothed_direction = Direction.DOWN
        if direction_history:
            from collections import Counter

            most_common_direction_tuple = Counter(direction_history).most_common(1)
            if most_common_direction_tuple:
                smoothed_direction = most_common_direction_tuple[0][0]
        else:
            if animation_mode == AnimationMode.IDLE:
                smoothed_direction = Direction.DOWN

        anim_key = f"{obj_id}_{display_id}"
        anim_properties = self._active_animations.get(anim_key)
        if anim_properties:
            animation_instance = anim_properties["animation_instance"]
            animation_instance.set_state(smoothed_direction, animation_mode, timestamp)
        else:
            logging.warning(
                f"Animation instance not found for key '{anim_key}'. Cannot set direction."
            )

    def update_all_active_animations(self, delta_time: float, current_timestamp: float):
        """
        Updates the animation frame for all currently active Animation instances.

        Args:
            delta_time (float): The time elapsed since the last update in seconds.
            current_timestamp (float): The current time (e.g., from time.time()).
        """
        for obj_id_display_id_key, anim_properties in self._active_animations.items():
            animation_instance = anim_properties["animation_instance"]
            animation_instance.update(dt=delta_time, timestamp=current_timestamp)

    def get_animation_properties(self, obj_id: str, display_id: str) -> dict | None:
        """
        Retrieves the animation properties for a given object ID and display ID, including the instance itself.

        Args:
            obj_id (str): The unique identifier of the object.
            display_id (str): The ID referencing the animation data.

        Returns:
            dict | None: A dictionary containing animation properties, or None if not found.
        """
        anim_key = f"{obj_id}_{display_id}"
        return self._active_animations.get(anim_key)

    def render_object_animation(
        self,
        obj_id: str,
        display_id: str,
        screen_x: float,
        screen_y: float,
        timestamp: float,
    ):
        """
        Renders the animation for a specific game object.
        This method retrieves the animation data from the managed Animation instance
        and delegates the actual drawing to the internal _render_animation_frame method.

        Args:
            obj_id (str): The unique identifier of the object to render.
            display_id (str): The ID referencing the animation data.
            screen_x (float): The X-coordinate on the screen for rendering.
            screen_y (float): The Y-coordinate on the screen for rendering.
            timestamp (float): The current time (e.g., from time.time()).
        """
        anim_properties = self.get_animation_properties(obj_id, display_id)
        if not anim_properties:
            logging.warning(
                f"No animation found for object ID: {obj_id} and display ID: {display_id}. Cannot render."
            )
            return

        animation_instance = anim_properties["animation_instance"]
        target_display_size_pixels = anim_properties["target_display_size_pixels"]

        frame_matrix, color_map, flip_horizontal, current_frame_index = (
            animation_instance.get_current_frame_data(timestamp)
        )

        self._render_animation_frame(
            frame_matrix=frame_matrix,
            color_map=color_map,
            screen_x=screen_x,
            screen_y=screen_y,
            display_size_pixels=target_display_size_pixels,
            flip_horizontal=flip_horizontal,
        )
