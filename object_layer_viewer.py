import logging
import time

from config import OBJECT_SIZE
from object_layer.animation_data import ANIMATION_DATA, AnimationMode, Direction
from object_layer.raylib_render import RaylibRender
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


class MockAnimationDataProvider:
    """
    A mock provider for animation data, emulating what a server might provide.
    """

    def get_animation_data(self) -> dict:
        return ANIMATION_DATA


if __name__ == "__main__":
    SCREEN_WIDTH = 800
    SCREEN_HEIGHT = 600
    TARGET_FPS = 60

    animation_data_provider = MockAnimationDataProvider()
    animation_data = animation_data_provider.get_animation_data()

    raylib_render = RaylibRender(
        screen_width=SCREEN_WIDTH,
        screen_height=SCREEN_HEIGHT,
        world_width=SCREEN_WIDTH,
        world_height=SCREEN_HEIGHT,
        object_size=OBJECT_SIZE,
        animation_data=animation_data,
        title="Rendering Viewer Demo",
        target_fps=TARGET_FPS,
    )

    AVAILABLE_OBJECT_LAYER_IDS = [
        "PEOPLE",
        "CLICK_POINTER",
        "POINT_PATH",
        "WALL",
    ]
    current_object_layer_id_index = 0
    demo_obj_id = "demo_animation_viewer_object"

    current_object_layer_id = AVAILABLE_OBJECT_LAYER_IDS[current_object_layer_id_index]
    demo_direction = Direction.DOWN
    animation_mode = AnimationMode.IDLE

    matrix_dimension = raylib_render.get_animation_matrix_dimension(
        current_object_layer_id
    )
    current_pixel_size_in_display = OBJECT_SIZE / matrix_dimension
    if current_pixel_size_in_display == 0:
        current_pixel_size_in_display = 1

    demo_animation_properties = raylib_render.get_or_create_animation(
        demo_obj_id,
        current_object_layer_id,
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
    print("Use '3' to switch to the next object layer ID and '4' for the previous.")
    print("Press ESC to close the window.")

    last_commanded_dx = 0.0
    last_commanded_dy = 0.0

    move_speed_sim = 5.0

    while not raylib_render.window_should_close():
        current_time = time.time()
        delta_time = current_time - last_frame_time
        last_frame_time = current_time

        if not demo_animation_instance.is_stateless:
            if not (
                raylib_render.is_key_pressed(KEY_UP)
                or raylib_render.is_key_pressed(KEY_KP_8)
                or raylib_render.is_key_pressed(KEY_RIGHT)
                or raylib_render.is_key_pressed(KEY_KP_6)
                or raylib_render.is_key_pressed(KEY_DOWN)
                or raylib_render.is_key_pressed(KEY_KP_2)
                or raylib_render.is_key_pressed(KEY_LEFT)
                or raylib_render.is_key_pressed(KEY_KP_4)
                or raylib_render.is_key_pressed(KEY_KP_7)
                or raylib_render.is_key_pressed(KEY_KP_9)
                or raylib_render.is_key_pressed(KEY_KP_1)
                or raylib_render.is_key_pressed(KEY_KP_3)
            ):
                if animation_mode == AnimationMode.IDLE:
                    last_commanded_dx = 0.0
                    last_commanded_dy = 0.0

            if raylib_render.is_key_pressed(KEY_UP) or raylib_render.is_key_pressed(
                KEY_KP_8
            ):
                last_commanded_dy = -move_speed_sim
                last_commanded_dx = 0.0
            if raylib_render.is_key_pressed(KEY_RIGHT) or raylib_render.is_key_pressed(
                KEY_KP_6
            ):
                last_commanded_dx = move_speed_sim
                last_commanded_dy = 0.0
            if raylib_render.is_key_pressed(KEY_DOWN) or raylib_render.is_key_pressed(
                KEY_KP_2
            ):
                last_commanded_dy = move_speed_sim
                last_commanded_dx = 0.0
            if raylib_render.is_key_pressed(KEY_LEFT) or raylib_render.is_key_pressed(
                KEY_KP_4
            ):
                last_commanded_dx = -move_speed_sim
                last_commanded_dy = 0.0

            if raylib_render.is_key_pressed(KEY_KP_7):
                last_commanded_dx = -move_speed_sim * 0.707
                last_commanded_dy = -move_speed_sim * 0.707
            if raylib_render.is_key_pressed(KEY_KP_9):
                last_commanded_dx = move_speed_sim * 0.707
                last_commanded_dy = -move_speed_sim * 0.707
            if raylib_render.is_key_pressed(KEY_KP_1):
                last_commanded_dx = -move_speed_sim * 0.707
                last_commanded_dy = move_speed_sim * 0.707
            if raylib_render.is_key_pressed(KEY_KP_3):
                last_commanded_dx = move_speed_sim * 0.707
                last_commanded_dy = move_speed_sim * 0.707

            if raylib_render.is_key_pressed(KEY_SPACE):
                animation_mode = (
                    AnimationMode.IDLE
                    if animation_mode == AnimationMode.WALKING
                    else AnimationMode.WALKING
                )
                if animation_mode == AnimationMode.IDLE:
                    last_commanded_dx = 0.0
                    last_commanded_dy = 0.0
                raylib_render.update_animation_direction_for_object(
                    obj_id=demo_obj_id,
                    object_layer_id=current_object_layer_id,
                    current_dx=last_commanded_dx,
                    current_dy=last_commanded_dy,
                    animation_mode=animation_mode,
                    timestamp=current_time,
                    reverse=True,
                )

            raylib_render.update_animation_direction_for_object(
                obj_id=demo_obj_id,
                object_layer_id=current_object_layer_id,
                current_dx=last_commanded_dx,
                current_dy=last_commanded_dy,
                animation_mode=animation_mode,
                timestamp=current_time,
                reverse=True,
            )
        else:
            last_commanded_dx = 0.0
            last_commanded_dy = 0.0
            animation_mode = AnimationMode.IDLE
            anim_properties = raylib_render.get_animation_properties(
                demo_obj_id, current_object_layer_id
            )
            if anim_properties:
                animation_instance = anim_properties["animation_instance"]
                animation_instance.set_state(
                    Direction.NONE, AnimationMode.IDLE, current_time
                )

        if raylib_render.is_key_pressed(KEY_TWO):
            current_pixel_size_in_display += 1.0
            raylib_render.get_or_create_animation(
                demo_obj_id,
                current_object_layer_id,
                current_pixel_size_in_display,
                initial_direction=demo_animation_instance.current_direction,
            )
        elif raylib_render.is_key_pressed(KEY_ONE):
            current_pixel_size_in_display -= 1.0
            if current_pixel_size_in_display < 1.0:
                current_pixel_size_in_display = 1.0
            raylib_render.get_or_create_animation(
                demo_obj_id,
                current_object_layer_id,
                current_pixel_size_in_display,
                initial_direction=demo_animation_instance.current_direction,
            )

        if raylib_render.is_key_pressed(KEY_THREE):
            current_object_layer_id_index = (current_object_layer_id_index + 1) % len(
                AVAILABLE_OBJECT_LAYER_IDS
            )
            current_object_layer_id = AVAILABLE_OBJECT_LAYER_IDS[
                current_object_layer_id_index
            ]
            raylib_render.remove_animation(obj_id=demo_obj_id, object_layer_id=None)
            animation_mode = AnimationMode.IDLE
            last_commanded_dx = 0.0
            last_commanded_dy = 0.0

            matrix_dimension = raylib_render.get_animation_matrix_dimension(
                current_object_layer_id
            )
            current_pixel_size_in_display = OBJECT_SIZE / matrix_dimension
            if current_pixel_size_in_display == 0:
                current_pixel_size_in_display = 1

            demo_animation_properties = raylib_render.get_or_create_animation(
                demo_obj_id,
                current_object_layer_id,
                current_pixel_size_in_display,
                initial_direction=Direction.DOWN,
            )
            demo_animation_instance = demo_animation_properties["animation_instance"]

        elif raylib_render.is_key_pressed(KEY_FOUR):
            current_object_layer_id_index = (
                current_object_layer_id_index - 1 + len(AVAILABLE_OBJECT_LAYER_IDS)
            ) % len(AVAILABLE_OBJECT_LAYER_IDS)
            current_object_layer_id = AVAILABLE_OBJECT_LAYER_IDS[
                current_object_layer_id_index
            ]
            raylib_render.remove_animation(obj_id=demo_obj_id, object_layer_id=None)
            animation_mode = AnimationMode.IDLE
            last_commanded_dx = 0.0
            last_commanded_dy = 0.0

            matrix_dimension = raylib_render.get_animation_matrix_dimension(
                current_object_layer_id
            )
            current_pixel_size_in_display = OBJECT_SIZE / matrix_dimension
            if current_pixel_size_in_display == 0:
                current_pixel_size_in_display = 1

            demo_animation_properties = raylib_render.get_or_create_animation(
                demo_obj_id,
                current_object_layer_id,
                current_pixel_size_in_display,
                initial_direction=Direction.DOWN,
            )
            demo_animation_instance = demo_animation_properties["animation_instance"]

        demo_animation_properties = raylib_render.get_animation_properties(
            demo_obj_id, current_object_layer_id
        )
        if not demo_animation_properties:
            continue

        demo_animation_instance = demo_animation_properties["animation_instance"]

        raylib_render.update_all_active_animations(delta_time, current_time)

        matrix_dimension = raylib_render.get_animation_matrix_dimension(
            current_object_layer_id
        )

        total_rendered_width = current_pixel_size_in_display * matrix_dimension
        total_rendered_height = current_pixel_size_in_display * matrix_dimension

        draw_x = (SCREEN_WIDTH / 2) - (total_rendered_width / 2)
        draw_y = (SCREEN_HEIGHT / 2) - (total_rendered_height / 2)

        raylib_render.begin_drawing()
        raylib_render.clear_background(Color(40, 40, 40, 255))

        raylib_render.render_object_animation(
            obj_id=demo_obj_id,
            object_layer_id=current_object_layer_id,
            screen_x=draw_x,
            screen_y=draw_y,
            timestamp=current_time,
        )

        raylib_render.draw_text(
            f"Current Object Layer ID: {current_object_layer_id}",
            10,
            10,
            20,
            Color(255, 255, 255, 255),
        )

        base_y_offset = 0
        if demo_animation_instance.is_stateless:
            raylib_render.draw_text(
                "Stateless Animation (Direction & Mode Ignored)",
                10,
                35,
                15,
                Color(200, 200, 200, 255),
            )
            base_y_offset = -70
        else:
            raylib_render.draw_text(
                f"Direction: {demo_animation_instance.current_direction.name}",
                10,
                35,
                20,
                Color(255, 255, 255, 255),
            )
            raylib_render.draw_text(
                f"Animation Mode: {demo_animation_instance.animation_mode.name.capitalize()}",
                10,
                60,
                15,
                Color(200, 200, 200, 255),
            )
            raylib_render.draw_text(
                "Press SPACE to toggle animation mode",
                10,
                80,
                15,
                Color(200, 200, 200, 255),
            )
            raylib_render.draw_text(
                "Use numpad/arrow keys to set direction",
                10,
                100,
                15,
                Color(200, 200, 200, 255),
            )

        raylib_render.draw_text(
            f"Pixel Size (per matrix element): {current_pixel_size_in_display:.2f}",
            10,
            125 + base_y_offset,
            15,
            Color(200, 200, 200, 255),
        )
        raylib_render.draw_text(
            "Use '1' for zoom out and '2' for zoom in",
            10,
            145 + base_y_offset,
            15,
            Color(200, 200, 200, 255),
        )
        raylib_render.draw_text(
            "Use '3'/'4' to change object layer ID",
            10,
            165 + base_y_offset,
            15,
            Color(200, 200, 200, 255),
        )
        raylib_render.draw_text(
            f"Frame Index (UI): {demo_animation_instance.current_frame_index}",
            10,
            190 + base_y_offset,
            15,
            Color(200, 200, 200, 255),
        )
        raylib_render.draw_text(
            f"Frame Timer (UI): {demo_animation_instance.frame_timer:.2f}",
            10,
            210 + base_y_offset,
            15,
            Color(200, 200, 200, 255),
        )

        raylib_render.draw_text(
            f"FPS: {int(1.0 / raylib_render.get_frame_time()) if raylib_render.get_frame_time() > 0 else 'N/A'}",
            10,
            SCREEN_HEIGHT - 30,
            20,
            Color(255, 255, 255, 255),
        )

        raylib_render.end_drawing()

    raylib_render.close_window()
