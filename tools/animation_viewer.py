import sys
import os
import time
import logging

# Add the project root to the Python path
# This allows imports like 'from core.raylib_manager import RaylibManager' to work
script_dir = os.path.dirname(__file__)
project_root = os.path.abspath(os.path.join(script_dir, ".."))
if project_root not in sys.path:
    sys.path.insert(0, project_root)

# Now import modules from the project
from core.raylib_manager import RaylibManager
from visuals.animation_manager import AnimationManager, Direction, AnimationMode
from raylibpy import (
    Color,
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
)

from data.animations.gfx.click_pointer import GFX_CLICK_POINTER_MATRIX_00
from data.animations.skin.people import SKIN_PEOPLE_MATRIX_08_0

# --- Logging Configuration ---
logging.basicConfig(
    level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s"
)

# --- Standalone Animation Viewer Logic ---
if __name__ == "__main__":
    SCREEN_WIDTH = 800
    SCREEN_HEIGHT = 600
    TARGET_FPS = 60

    INITIAL_OBJECT_BASE_SIZE = 50

    # Initialize Raylib through the manager
    raylib_manager = RaylibManager(
        SCREEN_WIDTH, SCREEN_HEIGHT, "Animation Viewer Demo", TARGET_FPS
    )

    # Initialize the AnimationManager, passing the RaylibManager
    animation_manager = AnimationManager(raylib_manager)

    AVAILABLE_DISPLAY_IDS = ["SKIN_PEOPLE", "GFX_CLICK_POINTER"]
    current_display_id_index = 0
    demo_obj_id = "demo_player_animation"  # Unique ID for the demo animation

    # Initial state for the demo player
    current_display_id = AVAILABLE_DISPLAY_IDS[current_display_id_index]
    current_direction = Direction.DOWN
    animation_mode = AnimationMode.IDLE

    # Calculate initial target display size based on base size
    current_target_display_size_pixels = 10

    # Create/get the initial animation state using the AnimationManager
    demo_animation_properties = animation_manager.get_or_create_animation(
        demo_obj_id,
        current_display_id,
        current_direction,
        animation_mode,
        current_target_display_size_pixels,
        time.time(),  # Pass current timestamp
    )
    # The actual Animation instance is nested within the properties dictionary
    demo_animation_instance = demo_animation_properties["animation_instance"]

    last_frame_time = time.time()

    print(
        "Use arrow keys or numpad directions (e.g., 8 for UP, 6 for RIGHT) to change animation direction."
    )
    print("Press SPACE to toggle animation mode (IDLE/WALKING).")
    print("Use '1' for zoom out and '2' for zoom in.")
    print("Use '3' to switch to the next animation ID and '4' for the previous.")
    print("Press ESC to close the window.")

    while (
        not raylib_manager.window_should_close()
    ):  # Use RaylibManager for window close check
        current_time = time.time()
        delta_time = current_time - last_frame_time
        last_frame_time = current_time

        # Get current animation properties (includes the instance)
        demo_animation_properties = animation_manager.get_animation_properties(
            demo_obj_id
        )
        if not demo_animation_properties:
            continue

        demo_animation_instance = demo_animation_properties["animation_instance"]
        current_display_id = demo_animation_properties["display_id"]

        # Update all active animations (only one in this demo)
        animation_manager.update_all_active_animations(delta_time, current_time)

        # --- Input Handling (using RaylibManager) ---
        new_direction_from_input = None

        if raylib_manager.is_key_pressed(KEY_UP) or raylib_manager.is_key_pressed(
            KEY_KP_8
        ):
            new_direction_from_input = Direction.UP
        elif raylib_manager.is_key_pressed(KEY_RIGHT) or raylib_manager.is_key_pressed(
            KEY_KP_6
        ):
            new_direction_from_input = Direction.RIGHT
        elif raylib_manager.is_key_pressed(KEY_DOWN) or raylib_manager.is_key_pressed(
            KEY_KP_2
        ):
            new_direction_from_input = Direction.DOWN
        elif raylib_manager.is_key_pressed(KEY_LEFT) or raylib_manager.is_key_pressed(
            KEY_KP_4
        ):
            new_direction_from_input = Direction.LEFT
        elif raylib_manager.is_key_pressed(KEY_KP_7):
            new_direction_from_input = Direction.UP_LEFT
        elif raylib_manager.is_key_pressed(KEY_KP_9):
            new_direction_from_input = Direction.UP_RIGHT
        elif raylib_manager.is_key_pressed(KEY_KP_1):
            new_direction_from_input = Direction.DOWN_LEFT
        elif raylib_manager.is_key_pressed(KEY_KP_3):
            new_direction_from_input = Direction.DOWN_RIGHT

        if new_direction_from_input:
            current_direction = new_direction_from_input
            animation_manager.get_or_create_animation(
                demo_obj_id,
                current_display_id,
                current_direction,
                animation_mode,
                current_target_display_size_pixels,
                current_time,
            )

        if raylib_manager.is_key_pressed(KEY_SPACE):
            animation_mode = (
                AnimationMode.IDLE
                if animation_mode == AnimationMode.WALKING
                else AnimationMode.WALKING
            )
            animation_manager.get_or_create_animation(
                demo_obj_id,
                current_display_id,
                current_direction,
                animation_mode,
                current_target_display_size_pixels,
                current_time,
            )

        if raylib_manager.is_key_pressed(KEY_TWO):

            current_target_display_size_pixels -= 1
            animation_manager.get_or_create_animation(
                demo_obj_id,
                current_display_id,
                current_direction,
                animation_mode,
                current_target_display_size_pixels,
                current_time,
            )
        elif raylib_manager.is_key_pressed(KEY_ONE):

            current_target_display_size_pixels += 1
            animation_manager.get_or_create_animation(
                demo_obj_id,
                current_display_id,
                current_direction,
                animation_mode,
                current_target_display_size_pixels,
                current_time,
            )

        if raylib_manager.is_key_pressed(KEY_THREE):  # Next ID
            current_display_id_index = (current_display_id_index + 1) % len(
                AVAILABLE_DISPLAY_IDS
            )
            current_display_id = AVAILABLE_DISPLAY_IDS[current_display_id_index]
            animation_manager.get_or_create_animation(
                demo_obj_id,
                current_display_id,
                current_direction,
                animation_mode,
                current_target_display_size_pixels,
                current_time,
            )
        elif raylib_manager.is_key_pressed(KEY_FOUR):  # Previous ID
            current_display_id_index = (
                current_display_id_index - 1 + len(AVAILABLE_DISPLAY_IDS)
            ) % len(AVAILABLE_DISPLAY_IDS)
            current_display_id = AVAILABLE_DISPLAY_IDS[current_display_id_index]
            animation_manager.get_or_create_animation(
                demo_obj_id,
                current_display_id,
                current_direction,
                animation_mode,
                current_target_display_size_pixels,
                current_time,
            )

        dim_num_pixels = 0
        if current_display_id == "GFX_CLICK_POINTER":
            dim_num_pixels = len(GFX_CLICK_POINTER_MATRIX_00)
        elif current_display_id == "SKIN_PEOPLE":
            dim_num_pixels = len(SKIN_PEOPLE_MATRIX_08_0)

        # Recalculate draw position based on current object display size
        draw_x = (SCREEN_WIDTH / 2) - (
            (current_target_display_size_pixels * dim_num_pixels) / 2
        )
        draw_y = (SCREEN_HEIGHT / 2) - (
            (current_target_display_size_pixels * dim_num_pixels) / 2
        )

        raylib_manager.begin_drawing()
        raylib_manager.clear_background(Color(40, 40, 40, 255))

        # Draw the demo sprite using the AnimationManager's render method
        animation_manager.render_object_animation(
            obj_id=demo_obj_id,
            screen_x=draw_x,
            screen_y=draw_y,
            timestamp=current_time,
        )

        # Display debug information (using RaylibManager)
        raylib_manager.draw_text(
            f"Current ID: {current_display_id}", 10, 10, 20, Color(255, 255, 255, 255)
        )

        # Display state from the animation instance itself
        raylib_manager.draw_text(
            f"Direction: {demo_animation_instance.current_direction.name}",
            10,
            35,
            20,
            Color(255, 255, 255, 255),
        )
        raylib_manager.draw_text(
            f"Animation Mode: {demo_animation_instance.animation_mode.name.capitalize()}",
            10,
            60,
            15,
            Color(200, 200, 200, 255),
        )

        if demo_animation_instance.is_stateless:
            raylib_manager.draw_text(
                "Directional input & SPACE ignored for this ID",
                10,
                80,
                15,
                Color(200, 200, 200, 255),
            )
        else:
            raylib_manager.draw_text(
                "Press SPACE to toggle animation mode",
                10,
                80,
                15,
                Color(200, 200, 200, 255),
            )
            raylib_manager.draw_text(
                "Use numpad/arrow keys to set direction",
                10,
                100,
                15,
                Color(200, 200, 200, 255),
            )

        raylib_manager.draw_text(
            f"Pixel Size: {current_target_display_size_pixels}",
            10,
            125,
            15,
            Color(200, 200, 200, 255),
        )
        raylib_manager.draw_text(
            "Use '1' for zoom out and '2' for zoom in",
            10,
            145,
            15,
            Color(200, 200, 200, 255),
        )
        raylib_manager.draw_text(
            "Use '3'/'4' to change animation ID", 10, 165, 15, Color(200, 200, 200, 255)
        )
        raylib_manager.draw_text(
            f"Frame Index (UI): {demo_animation_instance.current_frame_index}",
            10,
            190,
            15,
            Color(200, 200, 200, 255),
        )
        raylib_manager.draw_text(
            f"Frame Timer (UI): {demo_animation_instance.frame_timer:.2f}",
            10,
            210,
            15,
            Color(200, 200, 200, 255),
        )

        raylib_manager.draw_text(
            f"FPS: {int(1.0 / raylib_manager.get_frame_time()) if raylib_manager.get_frame_time() > 0 else 'N/A'}",
            10,
            SCREEN_HEIGHT - 30,
            20,
            Color(255, 255, 255, 255),
        )

        raylib_manager.end_drawing()

    raylib_manager.close_window()
