# cyberia-client/gameplay/items_engine.py

import time
from enum import Enum, auto
import logging

from raylibpy import (
    Color,
    draw_rectangle,
    init_window,
    set_target_fps,
    begin_drawing,
    clear_background,
    end_drawing,
    window_should_close,
    close_window,
    get_frame_time,
    is_key_pressed,
    KEY_UP,
    KEY_DOWN,
    KEY_LEFT,
    KEY_RIGHT,
    KEY_SPACE,
    KEY_KP_1,
    KEY_KP_2,
    KEY_KP_3,
    KEY_KP_4,
    KEY_KP_6,
    KEY_KP_7,
    KEY_KP_8,
    KEY_KP_9,
    KEY_ONE,
    KEY_TWO,
    KEY_THREE,
    KEY_FOUR,
    draw_text,
)

# Import config for global settings (if needed, currently not used directly in this file)
import config as settings  # Keep import for consistency, though not directly used here

# Imported matrices and color map for character animations
from items.skin.people import (
    SKIN_PEOPLE_MATRIX_08_0,
    SKIN_PEOPLE_MATRIX_08_1,  # UP IDLE
    SKIN_PEOPLE_MATRIX_06_0,
    SKIN_PEOPLE_MATRIX_06_1,  # RIGHT IDLE
    SKIN_PEOPLE_MATRIX_02_0,
    SKIN_PEOPLE_MATRIX_02_1,  # DOWN IDLE
    SKIN_PEOPLE_MATRIX_18_0,
    SKIN_PEOPLE_MATRIX_18_1,  # UP_RIGHT MOVE
    SKIN_PEOPLE_MATRIX_16_0,
    SKIN_PEOPLE_MATRIX_16_1,  # DOWN_RIGHT MOVE
    SKIN_PEOPLE_MATRIX_12_0,
    SKIN_PEOPLE_MATRIX_12_1,  # DOWN_LEFT MOVE
    SKIN_PEOPLE_MAP_COLORS,
)

# Imported matrices and color map for click pointer animation
from items.gfx.click_pointer import (
    GFX_CLICK_POINTER_MATRIX_00,
    GFX_CLICK_POINTER_MATRIX_01,
    GFX_CLICK_POINTER_MATRIX_02,
    GFX_CLICK_POINTER_MAP_COLORS,
    GFX_CLICK_POINTER_ANIMATION_SPEED,
)

# --- Logging Configuration ---
# Set to INFO to avoid verbose debug messages in console
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
# - 'frames': A nested dictionary mapping a string key (e.g., "up_idle", "right_move") to a list of matrices.
# - 'colors': The color map for these matrices.
# - 'frame_duration': The time in seconds each frame is displayed.
# - 'stop_delay_duration': Optional delay before transitioning from WALKING to IDLE.
# - 'dimensions': The base width and height of the sprite matrix (e.g., 26x26).
# - 'is_stateless': True if the animation does not use directional or idle/walking states.
ANIMATION_DATA = {
    "SKIN_PEOPLE": {
        "frames": {
            # IDLE animations (using 0X prefixed matrices)
            "up_idle": [SKIN_PEOPLE_MATRIX_08_0, SKIN_PEOPLE_MATRIX_08_1],
            "up_right_idle": [SKIN_PEOPLE_MATRIX_08_0, SKIN_PEOPLE_MATRIX_08_1],
            "right_idle": [SKIN_PEOPLE_MATRIX_06_0, SKIN_PEOPLE_MATRIX_06_1],
            "down_right_idle": [SKIN_PEOPLE_MATRIX_06_0, SKIN_PEOPLE_MATRIX_06_1],
            "down_idle": [SKIN_PEOPLE_MATRIX_02_0, SKIN_PEOPLE_MATRIX_02_1],
            "down_left_idle": [SKIN_PEOPLE_MATRIX_02_0, SKIN_PEOPLE_MATRIX_02_1],
            "left_idle": [SKIN_PEOPLE_MATRIX_06_0, SKIN_PEOPLE_MATRIX_06_1],
            "up_left_idle": [SKIN_PEOPLE_MATRIX_08_0, SKIN_PEOPLE_MATRIX_08_1],
            "default_idle": [
                SKIN_PEOPLE_MATRIX_02_0,
                SKIN_PEOPLE_MATRIX_02_1,
            ],  # Fallback
            # MOVE animations (using 1X prefixed matrices)
            "up_move": [SKIN_PEOPLE_MATRIX_18_0, SKIN_PEOPLE_MATRIX_18_1],
            "up_right_move": [SKIN_PEOPLE_MATRIX_18_0, SKIN_PEOPLE_MATRIX_18_1],
            "right_move": [SKIN_PEOPLE_MATRIX_16_0, SKIN_PEOPLE_MATRIX_16_1],
            "down_right_move": [SKIN_PEOPLE_MATRIX_16_0, SKIN_PEOPLE_MATRIX_16_1],
            "down_move": [SKIN_PEOPLE_MATRIX_12_0, SKIN_PEOPLE_MATRIX_12_1],
            "down_left_move": [SKIN_PEOPLE_MATRIX_12_0, SKIN_PEOPLE_MATRIX_12_1],
            "left_move": [SKIN_PEOPLE_MATRIX_16_0, SKIN_PEOPLE_MATRIX_16_1],
            "up_left_move": [SKIN_PEOPLE_MATRIX_18_0, SKIN_PEOPLE_MATRIX_18_1],
        },
        "colors": SKIN_PEOPLE_MAP_COLORS,
        "frame_duration": 0.3,  # Set to 1.5 seconds as requested
        "stop_delay_duration": 0.2,
        "dimensions": (26, 26),
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
        "stop_delay_duration": 0.0,
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
    Manages and renders pixel art animations from a set of matrices and a color map.
    Supports multiple frames per animation state (idle/moving) and direction.
    Handles stateless animations (e.g., UI elements) separately.
    """

    def __init__(
        self,
        frames_map: dict,
        color_map: list[Color],
        frame_duration: float = 0.15,
        stop_delay_duration: float = 0.2,
        is_stateless: bool = False,
    ):
        self.frames_map = frames_map
        self.color_map = color_map
        self.frame_duration = frame_duration
        self.stop_delay_duration = stop_delay_duration
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
        """
        new_direction = direction
        new_mode = mode

        if self.is_stateless:
            # Stateless animations always use NONE direction and WALKING mode to ensure continuous play
            new_direction = Direction.NONE
            new_mode = AnimationMode.WALKING

        state_changed = (self.current_direction != new_direction) or (
            self.animation_mode != new_mode
        )

        self.current_direction = new_direction
        self.animation_mode = new_mode

        if state_changed:
            self.current_frame_index = 0
            self.frame_timer = 0.0
            logging.info(
                f"Animation state changed to Direction: {self.current_direction.name}, Mode: {self.animation_mode.name}"
            )

        if self.animation_mode == AnimationMode.WALKING:
            self.last_moving_timestamp = timestamp

    def _get_current_animation_key(self, timestamp: float) -> str:
        """
        Determines the appropriate animation key (e.g., "up_idle", "right_move")
        based on the animation's internal state and movement status.
        """
        base_direction_key = DIRECTION_TO_KEY_PREFIX.get(
            self.current_direction, "default"
        )

        is_currently_moving = (self.animation_mode == AnimationMode.WALKING) or (
            timestamp - self.last_moving_timestamp < self.stop_delay_duration
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

    def render(
        self,
        screen_x: float,
        screen_y: float,
        display_size_pixels: int,  # The target width/height in screen pixels
        base_color: Color,  # Fallback color for missing pixel colors
        timestamp: float = 0.0,
    ):
        """
        Renders the current frame of the animation.
        """
        animation_key = self._get_current_animation_key(timestamp)
        frames_list = self.frames_map.get(animation_key)

        if not frames_list:
            logging.error(
                f"Render: No frames found for key '{animation_key}' or any fallback. Cannot draw."
            )
            return

        current_frame_matrix = frames_list[self.current_frame_index]

        matrix_dimension = len(current_frame_matrix)
        if matrix_dimension == 0:
            logging.warning(
                f"Render: Empty matrix for key '{animation_key}'. Cannot draw."
            )
            return

        # Calculate the size of each individual pixel in the scaled sprite.
        effective_pixel_size = max(1, int(display_size_pixels / matrix_dimension))

        # Determine if the sprite needs to be flipped horizontally.
        # Only flip for non-stateless animations and if facing left/up-left.
        flip_horizontal = False
        if not self.is_stateless and (
            self.current_direction == Direction.LEFT
            or self.current_direction == Direction.UP_LEFT
        ):
            flip_horizontal = True

        for row_idx in range(matrix_dimension):
            for col_idx in range(matrix_dimension):
                matrix_value = current_frame_matrix[row_idx][col_idx]

                cell_color = None
                if matrix_value > 0 and matrix_value < len(self.color_map):
                    cell_color = self.color_map[matrix_value]
                elif matrix_value != 0:
                    logging.debug(
                        f"Color mapping not found for matrix value: {matrix_value}. Using base_color."
                    )
                    cell_color = base_color

                if cell_color:
                    draw_col = col_idx
                    if flip_horizontal:
                        draw_col = matrix_dimension - 1 - col_idx

                    cell_draw_x = screen_x + draw_col * effective_pixel_size
                    cell_draw_y = screen_y + row_idx * effective_pixel_size

                    draw_rectangle(
                        int(cell_draw_x),
                        int(cell_draw_y),
                        effective_pixel_size,
                        effective_pixel_size,
                        cell_color,
                    )

        # --- Visual Animation Indicator (for debugging/demo) ---
        indicator_size = int(effective_pixel_size * 2)
        indicator_x = int(screen_x)
        indicator_y = int(screen_y)

        indicator_colors = [
            Color(255, 0, 0, 255),
            Color(0, 255, 0, 255),
            Color(0, 0, 255, 255),
            Color(255, 255, 0, 255),
        ]

        current_indicator_color = indicator_colors[
            self.current_frame_index % len(indicator_colors)
        ]
        draw_rectangle(
            indicator_x,
            indicator_y,
            indicator_size,
            indicator_size,
            current_indicator_color,
        )

        # --- On-Sprite Frame Index Display (for debugging/demo) ---
        text_size = int(display_size_pixels * 0.15)
        if text_size < 10:
            text_size = 10

        frame_text = str(self.current_frame_index)
        text_x = int(screen_x + effective_pixel_size)
        text_y = int(screen_y + effective_pixel_size)
        draw_text(frame_text, text_x, text_y, text_size, Color(255, 255, 255, 255))


# --- ItemsEngine Class ---
class ItemsEngine:
    """
    Manages and provides Animation instances for game objects.
    Acts as a factory for visual components, ensuring consistency and reusability.
    Caches active Animation instances and their associated display properties.
    """

    def __init__(self):
        # Cache for active Animation instances and their display properties (obj_id -> dict)
        self._active_animations: dict[str, dict] = {}

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
        """
        animation_info = ANIMATION_DATA.get(display_id)
        if not animation_info:
            raise ValueError(f"No animation data found for display_id '{display_id}'.")

        is_recreate_needed = False
        if obj_id in self._active_animations:
            current_cached_state = self._active_animations[obj_id]
            # Recreate if display_id or target_display_size_pixels has changed
            if (
                current_cached_state["display_id"] != display_id
                or current_cached_state["target_display_size_pixels"]
                != target_display_size_pixels
            ):
                is_recreate_needed = True
        else:
            is_recreate_needed = True  # Always create if object ID is new

        if is_recreate_needed:
            animation_instance = Animation(
                frames_map=animation_info["frames"],
                color_map=animation_info["colors"],
                frame_duration=animation_info["frame_duration"],
                stop_delay_duration=animation_info["stop_delay_duration"],
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
        else:
            animation_instance = self._active_animations[obj_id]["animation_instance"]

        # Always set the state, even if not recreated, to update direction/mode/timestamp
        animation_instance.set_state(desired_direction, desired_mode, timestamp)

        return self._active_animations[obj_id]

    def remove_animation(self, obj_id: str):
        """Removes an Animation instance from the cache when an object is removed."""
        if obj_id in self._active_animations:
            del self._active_animations[obj_id]
            logging.info(f"Removed animation for obj_id '{obj_id}'.")

    def update_all_active_animations(self, delta_time: float, current_timestamp: float):
        """
        Updates the animation frame for all currently active Animation instances.
        """
        for obj_id, anim_properties in self._active_animations.items():
            animation_instance = anim_properties["animation_instance"]
            animation_instance.update(dt=delta_time, timestamp=current_timestamp)

    def get_animation_properties(self, obj_id: str) -> dict | None:
        """Retrieves the animation properties for a given object ID, including the instance itself."""
        return self._active_animations.get(obj_id)


# --- Standalone Demo Logic ---
if __name__ == "__main__":
    SCREEN_WIDTH = 800
    SCREEN_HEIGHT = 600
    TARGET_FPS = 60

    INITIAL_OBJECT_BASE_SIZE = 50  # Base size for calculation, not actual pixel size
    current_zoom_factor = 7.0
    ZOOM_SPEED = 0.5
    MIN_ZOOM = 0.5
    MAX_ZOOM = 16.0

    init_window(SCREEN_WIDTH, SCREEN_HEIGHT, "Items Engine - Animation Demo")
    set_target_fps(TARGET_FPS)

    items_engine = ItemsEngine()

    AVAILABLE_DISPLAY_IDS = ["SKIN_PEOPLE", "GFX_CLICK_POINTER"]
    current_display_id_index = 0
    demo_obj_id = "demo_player"

    # Initial state for the demo player
    current_display_id = AVAILABLE_DISPLAY_IDS[current_display_id_index]
    current_direction = Direction.UP
    animation_mode = AnimationMode.IDLE

    # Calculate initial target display size based on base size and zoom
    current_target_display_size_pixels = int(
        INITIAL_OBJECT_BASE_SIZE * current_zoom_factor
    )

    # Create/get the initial animation state
    demo_animation_properties = items_engine.get_or_create_animation(
        demo_obj_id,
        current_display_id,
        current_direction,
        animation_mode,
        current_target_display_size_pixels,
        time.time(),  # Pass current timestamp
    )
    demo_animation_instance = demo_animation_properties["animation_instance"]

    last_frame_time = time.time()

    print(
        "Use arrow keys or numpad directions (e.g., 8 for UP, 6 for RIGHT) to change animation direction."
    )
    print("Press SPACE to toggle animation mode (IDLE/WALKING).")
    print("Use '1' for zoom out and '2' for zoom in.")
    print("Use '3' to switch to the next animation ID and '4' for the previous.")
    print("Press ESC to close the window.")

    while not window_should_close():
        current_time = time.time()
        delta_time = current_time - last_frame_time
        last_frame_time = current_time

        # Get current animation properties (includes the instance)
        demo_animation_properties = items_engine.get_animation_properties(demo_obj_id)
        if not demo_animation_properties:
            continue  # Should not happen after initial creation

        demo_animation_instance = demo_animation_properties["animation_instance"]
        current_display_id = demo_animation_properties["display_id"]
        current_target_display_size_pixels = demo_animation_properties[
            "target_display_size_pixels"
        ]

        # Update all active animations (only one in this demo)
        items_engine.update_all_active_animations(delta_time, current_time)

        # --- Input Handling ---
        new_direction_from_input = None
        if is_key_pressed(KEY_UP) or is_key_pressed(KEY_KP_8):
            new_direction_from_input = Direction.UP
        elif is_key_pressed(KEY_RIGHT) or is_key_pressed(KEY_KP_6):
            new_direction_from_input = Direction.RIGHT
        elif is_key_pressed(KEY_DOWN) or is_key_pressed(KEY_KP_2):
            new_direction_from_input = Direction.DOWN
        elif is_key_pressed(KEY_LEFT) or is_key_pressed(KEY_KP_4):
            new_direction_from_input = Direction.LEFT
        elif is_key_pressed(KEY_KP_7):
            new_direction_from_input = Direction.UP_LEFT
        elif is_key_pressed(KEY_KP_9):
            new_direction_from_input = Direction.UP_RIGHT
        elif is_key_pressed(KEY_KP_1):
            new_direction_from_input = Direction.DOWN_LEFT
        elif is_key_pressed(KEY_KP_3):
            new_direction_from_input = Direction.DOWN_RIGHT

        if new_direction_from_input:
            current_direction = new_direction_from_input
            items_engine.get_or_create_animation(
                demo_obj_id,
                current_display_id,
                current_direction,
                animation_mode,
                current_target_display_size_pixels,
                current_time,
            )

        if is_key_pressed(KEY_SPACE):
            animation_mode = (
                AnimationMode.IDLE
                if animation_mode == AnimationMode.WALKING
                else AnimationMode.WALKING
            )
            items_engine.get_or_create_animation(
                demo_obj_id,
                current_display_id,
                current_direction,
                animation_mode,
                current_target_display_size_pixels,
                current_time,
            )

        if is_key_pressed(KEY_TWO):  # Zoom In
            current_zoom_factor += ZOOM_SPEED
            if current_zoom_factor > MAX_ZOOM:
                current_zoom_factor = MAX_ZOOM
            current_target_display_size_pixels = int(
                INITIAL_OBJECT_BASE_SIZE * current_zoom_factor
            )
            items_engine.get_or_create_animation(
                demo_obj_id,
                current_display_id,
                current_direction,
                animation_mode,
                current_target_display_size_pixels,
                current_time,
            )
        elif is_key_pressed(KEY_ONE):  # Zoom Out
            current_zoom_factor -= ZOOM_SPEED
            if current_zoom_factor < MIN_ZOOM:
                current_zoom_factor = MIN_ZOOM
            current_target_display_size_pixels = int(
                INITIAL_OBJECT_BASE_SIZE * current_zoom_factor
            )
            items_engine.get_or_create_animation(
                demo_obj_id,
                current_display_id,
                current_direction,
                animation_mode,
                current_target_display_size_pixels,
                current_time,
            )

        if is_key_pressed(KEY_THREE):  # Next ID
            current_display_id_index = (current_display_id_index + 1) % len(
                AVAILABLE_DISPLAY_IDS
            )
            current_display_id = AVAILABLE_DISPLAY_IDS[current_display_id_index]
            items_engine.get_or_create_animation(
                demo_obj_id,
                current_display_id,
                current_direction,
                animation_mode,
                current_target_display_size_pixels,
                current_time,
            )
        elif is_key_pressed(KEY_FOUR):  # Previous ID
            current_display_id_index = (
                current_display_id_index - 1 + len(AVAILABLE_DISPLAY_IDS)
            ) % len(AVAILABLE_DISPLAY_IDS)
            current_display_id = AVAILABLE_DISPLAY_IDS[current_display_id_index]
            items_engine.get_or_create_animation(
                demo_obj_id,
                current_display_id,
                current_direction,
                animation_mode,
                current_target_display_size_pixels,
                current_time,
            )

        # Recalculate draw position based on current object display size
        draw_x = (SCREEN_WIDTH - current_target_display_size_pixels) / 2
        draw_y = (SCREEN_HEIGHT - current_target_display_size_pixels) / 2

        begin_drawing()
        clear_background(Color(40, 40, 40, 255))

        # Draw the demo sprite
        demo_animation_instance.render(
            screen_x=draw_x,
            screen_y=draw_y,
            display_size_pixels=current_target_display_size_pixels,
            base_color=Color(255, 255, 0, 255),
            timestamp=current_time,
        )

        # Display debug information
        draw_text(
            f"Current ID: {current_display_id}", 10, 10, 20, Color(255, 255, 255, 255)
        )

        # Display state from the animation instance itself
        draw_text(
            f"Direction: {demo_animation_instance.current_direction.name}",
            10,
            35,
            20,
            Color(255, 255, 255, 255),
        )
        draw_text(
            f"Animation Mode: {demo_animation_instance.animation_mode.name.capitalize()}",
            10,
            60,
            15,
            Color(200, 200, 200, 255),
        )

        if demo_animation_instance.is_stateless:
            draw_text(
                "Directional input & SPACE ignored for this ID",
                10,
                80,
                15,
                Color(200, 200, 200, 255),
            )
        else:
            draw_text(
                "Press SPACE to toggle animation mode",
                10,
                80,
                15,
                Color(200, 200, 200, 255),
            )
            draw_text(
                "Use numpad/arrow keys to set direction",
                10,
                100,
                15,
                Color(200, 200, 200, 255),
            )

        draw_text(
            f"Zoom: {current_zoom_factor:.1f}x (Object Size: {current_target_display_size_pixels})",
            10,
            125,
            15,
            Color(200, 200, 200, 255),
        )
        draw_text(
            "Use '1' for zoom out and '2' for zoom in",
            10,
            145,
            15,
            Color(200, 200, 200, 255),
        )
        draw_text(
            "Use '3'/'4' to change animation ID", 10, 165, 15, Color(200, 200, 200, 255)
        )
        draw_text(
            f"Frame Index (UI): {demo_animation_instance.current_frame_index}",
            10,
            190,
            15,
            Color(200, 200, 200, 255),
        )
        draw_text(
            f"Frame Timer (UI): {demo_animation_instance.frame_timer:.2f}",
            10,
            210,
            15,
            Color(200, 200, 200, 255),
        )

        draw_text(
            f"FPS: {int(1.0 / get_frame_time()) if get_frame_time() > 0 else 'N/A'}",
            10,
            SCREEN_HEIGHT - 30,
            20,
            Color(255, 255, 255, 255),
        )

        end_drawing()

    close_window()
