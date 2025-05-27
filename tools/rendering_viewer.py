import sys
import os
import time
import logging

# Add the project root to the Python path
script_dir = os.path.dirname(__file__)
project_root = os.path.abspath(os.path.join(script_dir, ".."))
if project_root not in sys.path:
    sys.path.insert(0, project_root)

from visuals.rendering_system import (
    RenderingSystem,
    Direction,
    AnimationMode,
    ANIMATION_DATA,
)
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

# Import object size from config
from config import OBJECT_SIZE


# --- Logging Configuration ---
logging.basicConfig(
    level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s"
)

# --- Standalone Rendering Viewer Logic ---
if __name__ == "__main__":
    SCREEN_WIDTH = 800
    SCREEN_HEIGHT = 600
    TARGET_FPS = 60

    # Initialize RenderingSystem with the global OBJECT_SIZE
    rendering_system = RenderingSystem(
        screen_width=SCREEN_WIDTH,
        screen_height=SCREEN_HEIGHT,
        world_width=SCREEN_WIDTH,
        world_height=SCREEN_HEIGHT,
        object_size=OBJECT_SIZE,  # Use the standard OBJECT_SIZE
        title="Rendering Viewer Demo",
        target_fps=TARGET_FPS,
    )

    # Use the new display IDs
    AVAILABLE_DISPLAY_IDS = [
        "PEOPLE_DISPLAY_ID",
        "CLICK_POINTER_DISPLAY_ID",
        "POINT_PATH_DISPLAY_ID",
        "WALL_DISPLAY_ID",
    ]
    current_display_id_index = 0
    demo_obj_id = "demo_animation_viewer_object"

    current_display_id = AVAILABLE_DISPLAY_IDS[current_display_id_index]
    demo_direction = Direction.DOWN
    animation_mode = AnimationMode.IDLE

    # Calculate initial target display size based on OBJECT_SIZE and the current animation's matrix dimensions
    # Dynamically get matrix dimension using the new method
    matrix_dimension = rendering_system.get_animation_matrix_dimension(
        current_display_id
    )
    # pixel_size_in_display is now calculated dynamically based on OBJECT_SIZE
    current_pixel_size_in_display = OBJECT_SIZE / matrix_dimension
    if current_pixel_size_in_display == 0:
        current_pixel_size_in_display = 1

    demo_animation_properties = rendering_system.get_or_create_animation(
        demo_obj_id,
        current_display_id,
        current_pixel_size_in_display,
        initial_direction=demo_direction,
    )
    demo_animation_instance = demo_animation_properties["animation_instance"]

    last_frame_time = time.time()

    print(
        "Use arrow keys or numpad directions (e.g., 8 for UP, 6 for RIGHT) to change animation direction."
    )
    print("Press SPACE to toggle animation mode (IDLE/WALKING).")
    print("Use '1' for zoom out and '2' for zoom in (changes individual pixel size).")
    print(
        "Use '3' to switch to the next display ID and '4' for the previous."
    )  # Updated text
    print("Press ESC to close the window.")

    last_commanded_dx = 0.0
    last_commanded_dy = 0.0

    move_speed_sim = 5.0  # Simulated movement speed for demo purposes

    while not rendering_system.window_should_close():
        current_time = time.time()
        delta_time = current_time - last_frame_time
        last_frame_time = current_time

        if not demo_animation_instance.is_stateless:
            key_pressed_for_movement = False

            # Handle directional input for non-stateless animations
            if not (
                rendering_system.is_key_pressed(KEY_UP)
                or rendering_system.is_key_pressed(KEY_KP_8)
                or rendering_system.is_key_pressed(KEY_RIGHT)
                or rendering_system.is_key_pressed(KEY_KP_6)
                or rendering_system.is_key_pressed(KEY_DOWN)
                or rendering_system.is_key_pressed(KEY_KP_2)
                or rendering_system.is_key_pressed(KEY_LEFT)
                or rendering_system.is_key_pressed(KEY_KP_4)
                or rendering_system.is_key_pressed(KEY_KP_7)
                or rendering_system.is_key_pressed(KEY_KP_9)
                or rendering_system.is_key_pressed(KEY_KP_1)
                or rendering_system.is_key_pressed(KEY_KP_3)
            ):
                if animation_mode == AnimationMode.IDLE:
                    last_commanded_dx = 0.0
                    last_commanded_dy = 0.0

            if rendering_system.is_key_pressed(
                KEY_UP
            ) or rendering_system.is_key_pressed(KEY_KP_8):
                last_commanded_dy = -move_speed_sim
                last_commanded_dx = 0.0
                key_pressed_for_movement = True
            if rendering_system.is_key_pressed(
                KEY_RIGHT
            ) or rendering_system.is_key_pressed(KEY_KP_6):
                last_commanded_dx = move_speed_sim
                last_commanded_dy = 0.0
                key_pressed_for_movement = True
            if rendering_system.is_key_pressed(
                KEY_DOWN
            ) or rendering_system.is_key_pressed(KEY_KP_2):
                last_commanded_dy = move_speed_sim
                last_commanded_dx = 0.0
                key_pressed_for_movement = True
            if rendering_system.is_key_pressed(
                KEY_LEFT
            ) or rendering_system.is_key_pressed(KEY_KP_4):
                last_commanded_dx = -move_speed_sim
                last_commanded_dy = 0.0
                key_pressed_for_movement = True

            if rendering_system.is_key_pressed(KEY_KP_7):  # UP_LEFT
                last_commanded_dx = -move_speed_sim * 0.707
                last_commanded_dy = -move_speed_sim * 0.707
                key_pressed_for_movement = True
            if rendering_system.is_key_pressed(KEY_KP_9):  # UP_RIGHT
                last_commanded_dx = move_speed_sim * 0.707
                last_commanded_dy = -move_speed_sim * 0.707
                key_pressed_for_movement = True
            if rendering_system.is_key_pressed(KEY_KP_1):  # DOWN_LEFT
                last_commanded_dx = -move_speed_sim * 0.707
                last_commanded_dy = move_speed_sim * 0.707
                key_pressed_for_movement = True
            if rendering_system.is_key_pressed(KEY_KP_3):  # DOWN_RIGHT
                last_commanded_dx = move_speed_sim * 0.707
                last_commanded_dy = move_speed_sim * 0.707
                key_pressed_for_movement = True

            if rendering_system.is_key_pressed(KEY_SPACE):
                animation_mode = (
                    AnimationMode.IDLE
                    if animation_mode == AnimationMode.WALKING
                    else AnimationMode.WALKING
                )
                if animation_mode == AnimationMode.IDLE:
                    last_commanded_dx = 0.0
                    last_commanded_dy = 0.0
                rendering_system.update_animation_direction_for_object(
                    obj_id=demo_obj_id,
                    display_id=current_display_id,
                    current_dx=last_commanded_dx,
                    current_dy=last_commanded_dy,
                    animation_mode=animation_mode,
                    timestamp=current_time,
                )

            rendering_system.update_animation_direction_for_object(
                obj_id=demo_obj_id,
                display_id=current_display_id,
                current_dx=last_commanded_dx,
                current_dy=last_commanded_dy,
                animation_mode=animation_mode,
                timestamp=current_time,
            )
        else:
            # For stateless animations, clear movement and set to IDLE
            last_commanded_dx = 0.0
            last_commanded_dy = 0.0
            animation_mode = AnimationMode.IDLE
            anim_properties = rendering_system.get_animation_properties(
                demo_obj_id, current_display_id
            )
            if anim_properties:
                animation_instance = anim_properties["animation_instance"]
                animation_instance.set_state(
                    Direction.NONE, AnimationMode.IDLE, current_time
                )

        # Handle scaling (pixel size) input
        if rendering_system.is_key_pressed(KEY_TWO):
            current_pixel_size_in_display += 1.0
            rendering_system.get_or_create_animation(
                demo_obj_id,
                current_display_id,
                current_pixel_size_in_display,
                initial_direction=demo_animation_instance.current_direction,
            )
        elif rendering_system.is_key_pressed(KEY_ONE):
            current_pixel_size_in_display -= 1.0
            if current_pixel_size_in_display < 1.0:
                current_pixel_size_in_display = 1.0
            rendering_system.get_or_create_animation(
                demo_obj_id,
                current_display_id,
                current_pixel_size_in_display,
                initial_direction=demo_animation_instance.current_direction,
            )

        # Handle display ID switching
        if rendering_system.is_key_pressed(KEY_THREE):
            current_display_id_index = (current_display_id_index + 1) % len(
                AVAILABLE_DISPLAY_IDS
            )
            current_display_id = AVAILABLE_DISPLAY_IDS[current_display_id_index]
            rendering_system.remove_animation(
                obj_id=demo_obj_id, display_id=None
            )  # Remove old animation
            animation_mode = AnimationMode.IDLE
            last_commanded_dx = 0.0
            last_commanded_dy = 0.0

            # Recalculate pixel size for the new display ID
            matrix_dimension = rendering_system.get_animation_matrix_dimension(
                current_display_id
            )
            current_pixel_size_in_display = OBJECT_SIZE / matrix_dimension
            if current_pixel_size_in_display == 0:
                current_pixel_size_in_display = 1

            demo_animation_properties = rendering_system.get_or_create_animation(
                demo_obj_id,
                current_display_id,
                current_pixel_size_in_display,
                initial_direction=Direction.DOWN,
            )
            demo_animation_instance = demo_animation_properties["animation_instance"]

        elif rendering_system.is_key_pressed(KEY_FOUR):
            current_display_id_index = (
                current_display_id_index - 1 + len(AVAILABLE_DISPLAY_IDS)
            ) % len(AVAILABLE_DISPLAY_IDS)
            current_display_id = AVAILABLE_DISPLAY_IDS[current_display_id_index]
            rendering_system.remove_animation(
                obj_id=demo_obj_id, display_id=None
            )  # Remove old animation
            animation_mode = AnimationMode.IDLE
            last_commanded_dx = 0.0
            last_commanded_dy = 0.0

            # Recalculate pixel size for the new display ID
            matrix_dimension = rendering_system.get_animation_matrix_dimension(
                current_display_id
            )
            current_pixel_size_in_display = OBJECT_SIZE / matrix_dimension
            if current_pixel_size_in_display == 0:
                current_pixel_size_in_display = 1

            demo_animation_properties = rendering_system.get_or_create_animation(
                demo_obj_id,
                current_display_id,
                current_pixel_size_in_display,
                initial_direction=Direction.DOWN,
            )
            demo_animation_instance = demo_animation_properties["animation_instance"]

        # Ensure demo_animation_properties is up to date after potential changes
        demo_animation_properties = rendering_system.get_animation_properties(
            demo_obj_id, current_display_id
        )
        if not demo_animation_properties:
            continue

        demo_animation_instance = demo_animation_properties["animation_instance"]

        rendering_system.update_all_active_animations(delta_time, current_time)

        # Get the actual dimensions using the new method
        matrix_dimension = rendering_system.get_animation_matrix_dimension(
            current_display_id
        )

        # Calculate total rendered size of the sprite
        total_rendered_width = current_pixel_size_in_display * matrix_dimension
        total_rendered_height = current_pixel_size_in_display * matrix_dimension

        # Center the animation on the screen
        draw_x = (SCREEN_WIDTH / 2) - (total_rendered_width / 2)
        draw_y = (SCREEN_HEIGHT / 2) - (total_rendered_height / 2)

        rendering_system.begin_drawing()
        rendering_system.clear_background(Color(40, 40, 40, 255))

        rendering_system.render_object_animation(
            obj_id=demo_obj_id,
            display_id=current_display_id,
            screen_x=draw_x,
            screen_y=draw_y,
            timestamp=current_time,
        )

        rendering_system.draw_text(
            f"Current Display ID: {current_display_id}",
            10,
            10,
            20,
            Color(255, 255, 255, 255),
        )

        base_y_offset = 0
        if demo_animation_instance.is_stateless:
            rendering_system.draw_text(
                "Stateless Animation (Direction & Mode Ignored)",
                10,
                35,
                15,
                Color(200, 200, 200, 255),
            )
            base_y_offset = -70
        else:
            rendering_system.draw_text(
                f"Direction: {demo_animation_instance.current_direction.name}",
                10,
                35,
                20,
                Color(255, 255, 255, 255),
            )
            rendering_system.draw_text(
                f"Animation Mode: {demo_animation_instance.animation_mode.name.capitalize()}",
                10,
                60,
                15,
                Color(200, 200, 200, 255),
            )
            rendering_system.draw_text(
                "Press SPACE to toggle animation mode",
                10,
                80,
                15,
                Color(200, 200, 200, 255),
            )
            rendering_system.draw_text(
                "Use numpad/arrow keys to set direction",
                10,
                100,
                15,
                Color(200, 200, 200, 255),
            )

        rendering_system.draw_text(
            f"Pixel Size (per matrix element): {current_pixel_size_in_display:.2f}",
            10,
            125 + base_y_offset,
            15,
            Color(200, 200, 200, 255),
        )
        rendering_system.draw_text(
            "Use '1' for zoom out and '2' for zoom in",
            10,
            145 + base_y_offset,
            15,
            Color(200, 200, 200, 255),
        )
        rendering_system.draw_text(
            "Use '3'/'4' to change display ID",
            10,
            165 + base_y_offset,
            15,
            Color(200, 200, 200, 255),
        )
        rendering_system.draw_text(
            f"Frame Index (UI): {demo_animation_instance.current_frame_index}",
            10,
            190 + base_y_offset,
            15,
            Color(200, 200, 200, 255),
        )
        rendering_system.draw_text(
            f"Frame Timer (UI): {demo_animation_instance.frame_timer:.2f}",
            10,
            210 + base_y_offset,
            15,
            Color(200, 200, 200, 255),
        )

        rendering_system.draw_text(
            f"FPS: {int(1.0 / rendering_system.get_frame_time()) if rendering_system.get_frame_time() > 0 else 'N/A'}",
            10,
            SCREEN_HEIGHT - 30,
            20,
            Color(255, 255, 255, 255),
        )

        rendering_system.end_drawing()

    rendering_system.close_window()
