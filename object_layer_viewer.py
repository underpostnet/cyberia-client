import logging
import time

from config import NETWORK_OBJECT_SIZE
from object_layer.object_layer_data import OBJECT_LAYER_DATA, ObjectLayerMode, Direction
from object_layer.object_layer_render import ObjectLayerRender
from raylibpy import (
    KEY_DOWN,
    KEY_FOUR,
    KEY_KP_1,
    KEY_KP_2,
    KEY_KP_3,
    KEY_KP_4,
    KEY_KP_6,
    KEY_KP_7,
    KEY_KP_8,
    KEY_KP_9,
    KEY_LEFT,
    KEY_ONE,
    KEY_RIGHT,
    KEY_SPACE,
    KEY_THREE,
    KEY_TWO,
    KEY_UP,
    Color,
)

logging.basicConfig(
    level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s"
)


class MockObjectLayerDataProvider:
    """Provides object layer data for the viewer demo."""

    def get_object_layer_data(self) -> dict:
        return OBJECT_LAYER_DATA


if __name__ == "__main__":
    SCREEN_WIDTH = 800
    SCREEN_HEIGHT = 600
    TARGET_FPS = 60

    object_layer_data_provider = MockObjectLayerDataProvider()
    object_layer_data = object_layer_data_provider.get_object_layer_data()

    object_layer_render = ObjectLayerRender(
        screen_width=SCREEN_WIDTH,
        screen_height=SCREEN_HEIGHT,
        world_width=SCREEN_WIDTH,  # For demo, world matches screen size
        world_height=SCREEN_HEIGHT,
        network_object_size=NETWORK_OBJECT_SIZE,
        object_layer_data=object_layer_data,
        title="Object Layer Viewer Demo",
        target_fps=TARGET_FPS,
    )

    AVAILABLE_OBJECT_LAYER_IDS = [
        "PEOPLE",
        "CLICK_POINTER",
        "POINT_PATH",
        "WALL",
    ]
    current_object_layer_id_index = 0
    demo_obj_id = "demo_object_layer_viewer_object"  # Unique ID for the demo object

    current_object_layer_id = AVAILABLE_OBJECT_LAYER_IDS[current_object_layer_id_index]
    demo_direction = Direction.DOWN
    object_layer_mode = ObjectLayerMode.IDLE

    # Calculate initial pixel size based on network object size and matrix dimension
    matrix_dimension = object_layer_render.get_object_layer_matrix_dimension(
        current_object_layer_id
    )
    current_pixel_size_in_display = NETWORK_OBJECT_SIZE / matrix_dimension
    if current_pixel_size_in_display == 0:
        current_pixel_size_in_display = 1

    # Get or create the animation instance for the demo object
    demo_object_layer_properties = (
        object_layer_render.get_or_create_object_layer_animation(
            demo_obj_id,
            current_object_layer_id,
            current_pixel_size_in_display,
            initial_direction=demo_direction,
        )
    )
    demo_object_layer_animation_instance = demo_object_layer_properties[
        "object_layer_animation_instance"
    ]

    last_frame_time = time.time()

    print("--- Object Layer Viewer Controls ---")
    print(
        "Use arrow keys or numpad directions (e.g., 8 for UP, 6 for RIGHT) to change object layer animation direction."
    )
    print("Press SPACE to toggle object layer animation mode (IDLE/WALKING).")
    print("Use '1' for zoom out and '2' for zoom in (changes individual pixel size).")
    print("Use '3' to switch to the next object layer ID and '4' for the previous.")
    print("Press ESC to close the window.")

    last_commanded_dx = 0.0
    last_commanded_dy = 0.0
    move_speed_sim = 5.0  # Simulated movement speed for demo

    while not object_layer_render.window_should_close():
        current_time = time.time()
        delta_time = current_time - last_frame_time
        last_frame_time = current_time

        # Handle input for non-stateless animations (e.g., player character)
        if not demo_object_layer_animation_instance.is_stateless:
            # Reset movement if no direction keys are pressed and in IDLE mode
            if not (
                object_layer_render.is_key_pressed(KEY_UP)
                or object_layer_render.is_key_pressed(KEY_KP_8)
                or object_layer_render.is_key_pressed(KEY_RIGHT)
                or object_layer_render.is_key_pressed(KEY_KP_6)
                or object_layer_render.is_key_pressed(KEY_DOWN)
                or object_layer_render.is_key_pressed(KEY_KP_2)
                or object_layer_render.is_key_pressed(KEY_LEFT)
                or object_layer_render.is_key_pressed(KEY_KP_4)
                or object_layer_render.is_key_pressed(KEY_KP_7)
                or object_layer_render.is_key_pressed(KEY_KP_9)
                or object_layer_render.is_key_pressed(KEY_KP_1)
                or object_layer_render.is_key_pressed(KEY_KP_3)
            ):
                if object_layer_mode == ObjectLayerMode.IDLE:
                    last_commanded_dx = 0.0
                    last_commanded_dy = 0.0

            # Update commanded movement based on key presses
            if object_layer_render.is_key_pressed(
                KEY_UP
            ) or object_layer_render.is_key_pressed(KEY_KP_8):
                last_commanded_dy = -move_speed_sim
                last_commanded_dx = 0.0
            if object_layer_render.is_key_pressed(
                KEY_RIGHT
            ) or object_layer_render.is_key_pressed(KEY_KP_6):
                last_commanded_dx = move_speed_sim
                last_commanded_dy = 0.0
            if object_layer_render.is_key_pressed(
                KEY_DOWN
            ) or object_layer_render.is_key_pressed(KEY_KP_2):
                last_commanded_dy = move_speed_sim
                last_commanded_dx = 0.0
            if object_layer_render.is_key_pressed(
                KEY_LEFT
            ) or object_layer_render.is_key_pressed(KEY_KP_4):
                last_commanded_dx = -move_speed_sim
                last_commanded_dy = 0.0

            # Diagonal movements
            if object_layer_render.is_key_pressed(KEY_KP_7):  # Up-Left
                last_commanded_dx = -move_speed_sim * 0.707
                last_commanded_dy = -move_speed_sim * 0.707
            if object_layer_render.is_key_pressed(KEY_KP_9):  # Up-Right
                last_commanded_dx = move_speed_sim * 0.707
                last_commanded_dy = -move_speed_sim * 0.707
            if object_layer_render.is_key_pressed(KEY_KP_1):  # Down-Left
                last_commanded_dx = -move_speed_sim * 0.707
                last_commanded_dy = move_speed_sim * 0.707
            if object_layer_render.is_key_pressed(KEY_KP_3):  # Down-Right
                last_commanded_dx = move_speed_sim * 0.707
                last_commanded_dy = move_speed_sim * 0.707

            # Toggle walking/idle mode
            if object_layer_render.is_key_pressed(KEY_SPACE):
                object_layer_mode = (
                    ObjectLayerMode.IDLE
                    if object_layer_mode == ObjectLayerMode.WALKING
                    else ObjectLayerMode.WALKING
                )
                if object_layer_mode == ObjectLayerMode.IDLE:
                    last_commanded_dx = 0.0
                    last_commanded_dy = 0.0
                # Update animation direction and mode
                object_layer_render.update_object_layer_direction_for_object(
                    obj_id=demo_obj_id,
                    object_layer_id=current_object_layer_id,
                    current_dx=last_commanded_dx,
                    current_dy=last_commanded_dy,
                    object_layer_mode=object_layer_mode,
                    timestamp=current_time,
                    reverse=True,  # Reverse direction for display in viewer
                )

            # Continuous update of animation direction based on commanded movement
            object_layer_render.update_object_layer_direction_for_object(
                obj_id=demo_obj_id,
                object_layer_id=current_object_layer_id,
                current_dx=last_commanded_dx,
                current_dy=last_commanded_dy,
                object_layer_mode=object_layer_mode,
                timestamp=current_time,
                reverse=True,  # Reverse direction for display in viewer
            )
        else:
            # For stateless animations, reset movement and mode
            last_commanded_dx = 0.0
            last_commanded_dy = 0.0
            object_layer_mode = ObjectLayerMode.IDLE
            anim_properties = object_layer_render.get_object_layer_animation_properties(
                demo_obj_id, current_object_layer_id
            )
            if anim_properties:
                anim_instance = anim_properties["object_layer_animation_instance"]
                anim_instance.set_state(
                    Direction.NONE, ObjectLayerMode.IDLE, current_time
                )

        # Handle zoom (pixel size) changes
        if object_layer_render.is_key_pressed(KEY_TWO):
            current_pixel_size_in_display += 1.0
            object_layer_render.get_or_create_object_layer_animation(
                demo_obj_id,
                current_object_layer_id,
                current_pixel_size_in_display,
                initial_direction=demo_object_layer_animation_instance.current_direction,
            )
        elif object_layer_render.is_key_pressed(KEY_ONE):
            current_pixel_size_in_display -= 1.0
            if current_pixel_size_in_display < 1.0:
                current_pixel_size_in_display = 1.0
            object_layer_render.get_or_create_object_layer_animation(
                demo_obj_id,
                current_object_layer_id,
                current_pixel_size_in_display,
                initial_direction=demo_object_layer_animation_instance.current_direction,
            )

        # Handle object layer ID changes
        if object_layer_render.is_key_pressed(KEY_THREE):
            current_object_layer_id_index = (current_object_layer_id_index + 1) % len(
                AVAILABLE_OBJECT_LAYER_IDS
            )
            current_object_layer_id = AVAILABLE_OBJECT_LAYER_IDS[
                current_object_layer_id_index
            ]
            object_layer_render.remove_object_layer_animation(
                obj_id=demo_obj_id, object_layer_id=None
            )  # Remove all animations for this obj_id
            object_layer_mode = ObjectLayerMode.IDLE
            last_commanded_dx = 0.0
            last_commanded_dy = 0.0

            matrix_dimension = object_layer_render.get_object_layer_matrix_dimension(
                current_object_layer_id
            )
            current_pixel_size_in_display = NETWORK_OBJECT_SIZE / matrix_dimension
            if current_pixel_size_in_display == 0:
                current_pixel_size_in_display = 1

            demo_object_layer_properties = (
                object_layer_render.get_or_create_object_layer_animation(
                    demo_obj_id,
                    current_object_layer_id,
                    current_pixel_size_in_display,
                    initial_direction=Direction.DOWN,
                )
            )
            demo_object_layer_animation_instance = demo_object_layer_properties[
                "object_layer_animation_instance"
            ]

        elif object_layer_render.is_key_pressed(KEY_FOUR):
            current_object_layer_id_index = (
                current_object_layer_id_index - 1 + len(AVAILABLE_OBJECT_LAYER_IDS)
            ) % len(AVAILABLE_OBJECT_LAYER_IDS)
            current_object_layer_id = AVAILABLE_OBJECT_LAYER_IDS[
                current_object_layer_id_index
            ]
            object_layer_render.remove_object_layer_animation(
                obj_id=demo_obj_id, object_layer_id=None
            )  # Remove all animations for this obj_id
            object_layer_mode = ObjectLayerMode.IDLE
            last_commanded_dx = 0.0
            last_commanded_dy = 0.0

            matrix_dimension = object_layer_render.get_object_layer_matrix_dimension(
                current_object_layer_id
            )
            current_pixel_size_in_display = NETWORK_OBJECT_SIZE / matrix_dimension
            if current_pixel_size_in_display == 0:
                current_pixel_size_in_display = 1

            demo_object_layer_properties = (
                object_layer_render.get_or_create_object_layer_animation(
                    demo_obj_id,
                    current_object_layer_id,
                    current_pixel_size_in_display,
                    initial_direction=Direction.DOWN,
                )
            )
            demo_object_layer_animation_instance = demo_object_layer_properties[
                "object_layer_animation_instance"
            ]

        # Get current animation instance properties
        demo_object_layer_properties = (
            object_layer_render.get_object_layer_animation_properties(
                demo_obj_id, current_object_layer_id
            )
        )
        if not demo_object_layer_properties:
            continue

        demo_object_layer_animation_instance = demo_object_layer_properties[
            "object_layer_animation_instance"
        ]

        # Update all active animations (only one in this demo)
        object_layer_render.update_all_active_object_layer_animations(
            delta_time, current_time
        )

        # Calculate drawing position for the demo object
        matrix_dimension = object_layer_render.get_object_layer_matrix_dimension(
            current_object_layer_id
        )
        total_rendered_width = current_pixel_size_in_display * matrix_dimension
        total_rendered_height = current_pixel_size_in_display * matrix_dimension
        draw_x = (SCREEN_WIDTH / 2) - (total_rendered_width / 2)
        draw_y = (SCREEN_HEIGHT / 2) - (total_rendered_height / 2)

        # --- Rendering ---
        object_layer_render.begin_drawing()
        object_layer_render.clear_background(Color(40, 40, 40, 255))  # Dark background

        # Render the demo object's animation
        object_layer_render.render_object_layer_animation(
            obj_id=demo_obj_id,
            object_layer_id=current_object_layer_id,
            screen_x=draw_x,
            screen_y=draw_y,
            timestamp=current_time,
        )

        # Display UI text
        object_layer_render.draw_text(
            f"Current Object Layer ID: {current_object_layer_id}",
            10,
            10,
            20,
            Color(255, 255, 255, 255),
        )

        base_y_offset = 0
        if demo_object_layer_animation_instance.is_stateless:
            object_layer_render.draw_text(
                "Stateless Animation (Direction & Mode Ignored)",
                10,
                35,
                15,
                Color(200, 200, 200, 255),
            )
            base_y_offset = -70  # Adjust UI position for stateless info
        else:
            object_layer_render.draw_text(
                f"Direction: {demo_object_layer_animation_instance.current_direction.name}",
                10,
                35,
                20,
                Color(255, 255, 255, 255),
            )
            object_layer_render.draw_text(
                f"Mode: {demo_object_layer_animation_instance.object_layer_mode.name.capitalize()}",
                10,
                60,
                15,
                Color(200, 200, 200, 255),
            )
            object_layer_render.draw_text(
                "Press SPACE to toggle mode",
                10,
                80,
                15,
                Color(200, 200, 200, 255),
            )
            object_layer_render.draw_text(
                "Use numpad/arrow keys to set direction",
                10,
                100,
                15,
                Color(200, 200, 200, 255),
            )

        object_layer_render.draw_text(
            f"Pixel Size: {current_pixel_size_in_display:.2f}",
            10,
            125 + base_y_offset,
            15,
            Color(200, 200, 200, 255),
        )
        object_layer_render.draw_text(
            "Use '1' for zoom out and '2' for zoom in",
            10,
            145 + base_y_offset,
            15,
            Color(200, 200, 200, 255),
        )
        object_layer_render.draw_text(
            "Use '3'/'4' to change object layer ID",
            10,
            165 + base_y_offset,
            15,
            Color(200, 200, 200, 255),
        )
        object_layer_render.draw_text(
            f"Frame Index: {demo_object_layer_animation_instance.current_frame_index}",
            10,
            190 + base_y_offset,
            15,
            Color(200, 200, 200, 255),
        )
        object_layer_render.draw_text(
            f"Frame Timer: {demo_object_layer_animation_instance.frame_timer:.2f}",
            10,
            210 + base_y_offset,
            15,
            Color(200, 200, 200, 255),
        )

        object_layer_render.draw_text(
            f"FPS: {int(1.0 / object_layer_render.get_frame_time()) if object_layer_render.get_frame_time() > 0 else 'N/A'}",
            10,
            SCREEN_HEIGHT - 30,
            20,
            Color(255, 255, 255, 255),
        )

        object_layer_render.end_drawing()

    object_layer_render.close_window()
