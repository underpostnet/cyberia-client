import logging
import time
import copy  # Import copy for deep copying

from config import NETWORK_OBJECT_SIZE
from object_layer.object_layer_data import OBJECT_LAYER_DATA, ObjectLayerMode, Direction
from object_layer.object_layer_render import ObjectLayerRender
from raylibpy import (
    KEY_DOWN,
    KEY_KP_1,
    KEY_KP_2,
    KEY_KP_3,
    KEY_KP_4,
    KEY_KP_6,
    KEY_KP_7,
    KEY_KP_8,
    KEY_KP_9,
    KEY_LEFT,
    KEY_RIGHT,
    KEY_SPACE,
    KEY_UP,
    Color,
    KEY_Q,
    KEY_W,
    KEY_E,
    KEY_R,
    KEY_ENTER,
    KEY_ZERO,
    KEY_ONE,
    KEY_TWO,
    KEY_THREE,
    KEY_FOUR,
    KEY_FIVE,
    KEY_SIX,
    KEY_SEVEN,
    KEY_EIGHT,
    KEY_NINE,
    KEY_T,  # Import KEY_T for test mode
    MOUSE_BUTTON_LEFT,
    KEY_BACKSPACE,  # Import KEY_BACKSPACE
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
    original_object_layer_data = object_layer_data_provider.get_object_layer_data()

    object_layer_render = ObjectLayerRender(
        screen_width=SCREEN_WIDTH,
        screen_height=SCREEN_HEIGHT,
        world_width=SCREEN_WIDTH,  # For demo, world matches screen size
        world_height=SCREEN_HEIGHT,
        network_object_size=NETWORK_OBJECT_SIZE,
        object_layer_data=original_object_layer_data,  # Initially use original data
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

    # --- New State Variables for Edit Mode and Clones ---
    is_edit_mode: bool = False
    is_test_mode: bool = False  # New state for test mode
    # Stores cloned data: {object_layer_id: {"RENDER_DATA": {...}, "SEED_DATA": {...}, "MODIFIED": bool}}
    cloned_object_layer_states: dict[str, dict] = {}

    # Function to get the current active render data (original or clone)
    def get_active_object_layer_render_data(obj_id: str, obj_layer_id: str) -> dict:
        if is_edit_mode and obj_layer_id in cloned_object_layer_states:
            return cloned_object_layer_states[obj_layer_id]["RENDER_DATA"]
        return original_object_layer_data[obj_layer_id]["RENDER_DATA"]

    # Function to check if the current object layer has been modified in clone
    def is_current_object_layer_modified() -> bool:
        return is_edit_mode and cloned_object_layer_states.get(
            current_object_layer_id, {}
        ).get("MODIFIED", False)

    # Calculate initial pixel size based on network object size and matrix dimension
    matrix_dimension = object_layer_render.get_object_layer_matrix_dimension(
        current_object_layer_id
    )
    current_pixel_size_in_display = NETWORK_OBJECT_SIZE / matrix_dimension
    if current_pixel_size_in_display == 0:
        current_pixel_size_in_display = 1

    # Get or create the animation instance for the demo object
    # This instance will be dynamically updated with original/clone data
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

    # Helper function to update the animation instance's data source based on current mode and test mode
    # This function is now defined AFTER demo_object_layer_animation_instance is initialized.
    def update_animation_instance_data_source():
        # These variables are in the global scope of this script, so 'global' is used.
        global demo_object_layer_animation_instance
        global current_object_layer_id
        global original_object_layer_data
        global cloned_object_layer_states
        global is_edit_mode
        global is_test_mode

        target_render_data = None
        if is_edit_mode:
            # In edit mode, always use the clone's data if it exists
            if current_object_layer_id in cloned_object_layer_states:
                target_render_data = cloned_object_layer_states[
                    current_object_layer_id
                ]["RENDER_DATA"]
            else:
                # Should ideally not happen if clone is created on entering edit mode
                target_render_data = original_object_layer_data[
                    current_object_layer_id
                ]["RENDER_DATA"]
                logging.warning(
                    f"No clone found for {current_object_layer_id} in edit mode, using original."
                )
        else:  # Normal Mode
            if is_test_mode and current_object_layer_id in cloned_object_layer_states:
                target_render_data = cloned_object_layer_states[
                    current_object_layer_id
                ]["RENDER_DATA"]
            else:
                target_render_data = original_object_layer_data[
                    current_object_layer_id
                ]["RENDER_DATA"]

        if target_render_data:
            demo_object_layer_animation_instance.set_frames_and_colors(
                target_render_data["FRAMES"], target_render_data["COLORS"]
            )
        else:
            logging.error(
                f"Could not determine render data for {current_object_layer_id}. Using default."
            )
            # Fallback to original if no target_render_data could be determined
            default_render_data = original_object_layer_data[current_object_layer_id][
                "RENDER_DATA"
            ]
            demo_object_layer_animation_instance.set_frames_and_colors(
                default_render_data["FRAMES"], default_render_data["COLORS"]
            )

    # Initial update of animation instance data source
    update_animation_instance_data_source()

    last_frame_time = time.time()

    print("--- Object Layer Viewer Controls ---")
    print(
        "Use arrow keys or numpad directions (e.g., 8 for UP, 6 for RIGHT) to change object layer animation direction."
    )
    print("Press SPACE to toggle object layer animation mode (IDLE/WALKING).")
    print("Use 'Q' for zoom out and 'W' for zoom in (changes individual pixel size).")
    print("Use 'E' to switch to the next object layer ID and 'R' for the previous.")
    print(
        "Use number keys (0-9) to enter EDIT MODE and pause animation at a specific frame."
    )
    print("Press ENTER to return to NORMAL MODE and resume animation.")
    print(
        "In EDIT MODE, click on the animation grid to change pixel colors with the selected palette color."
    )
    print("Press BACKSPACE to reset the current clone to its original state.")
    print("Press 'T' to toggle TEST MODE (runs cloned animation in Normal Mode).")
    print("Press ESC to close the window.")

    last_commanded_dx = 0.0
    last_commanded_dy = 0.0
    move_speed_sim = 5.0  # Simulated movement speed for demo

    # Define a fixed 16-color rainbow palette
    def get_rainbow_color(index, total_colors):
        """Generates a rainbow color based on index."""
        hue = index * (360 / total_colors)  # Hue in degrees (0-360)

        # Simple HSL to RGB conversion for a vibrant spectrum
        c = 1.0  # Chroma (saturation * lightness)
        s = 1.0  # Saturation
        l = 0.5  # Lightness (fixed for vibrant colors)

        h_prime = hue / 60.0
        x = c * (1 - abs(h_prime % 2 - 1))

        r1, g1, b1 = 0, 0, 0
        if 0 <= h_prime < 1:
            r1, g1, b1 = c, x, 0
        elif 1 <= h_prime < 2:
            r1, g1, b1 = x, c, 0
        elif 2 <= h_prime < 3:
            r1, g1, b1 = 0, c, x
        elif 3 <= h_prime < 4:
            r1, g1, b1 = 0, x, c
        elif 4 <= h_prime < 5:
            r1, g1, b1 = x, 0, c
        elif 5 <= h_prime < 6:
            r1, g1, b1 = c, 0, x

        m = l - c / 2
        r, g, b = (r1 + m), (g1 + m), (b1 + m)

        return Color(int(r * 255), int(g * 255), int(b * 255), 255)

    def get_darker_color(color: Color, factor: float = 0.7) -> Color:
        """Returns a darker version of the given color."""
        return Color(
            int(color.r * factor),
            int(color.g * factor),
            int(color.b * factor),
            color.a,
        )

    COLOR_PALETTE_RAINBOW = [get_rainbow_color(i, 16) for i in range(16)]
    COLOR_PALETTE_RAINBOW_DARKER_1 = [
        get_darker_color(color, 0.7) for color in COLOR_PALETTE_RAINBOW
    ]
    COLOR_PALETTE_RAINBOW_DARKER_2 = [
        get_darker_color(color, 0.4) for color in COLOR_PALETTE_RAINBOW
    ]

    # Define a 16-color lightness/darkness bar (white to black)
    COLOR_PALETTE_LIGHTNESS = [
        Color(
            int(255 - i * (255 / 15)),
            int(255 - i * (255 / 15)),
            int(255 - i * (255 / 15)),
            255,
        )
        for i in range(16)
    ]

    PALETTE_SQUARE_SIZE = 20
    PALETTE_PADDING = 10
    PALETTE_COLS_RAINBOW = 16
    PALETTE_ROWS_RAINBOW = 1
    PALETTE_COLS_LIGHTNESS = 16
    PALETTE_ROWS_LIGHTNESS = 1

    # Initialize selected color to white
    selected_color_rgb_text = "Selected: R:255 G:255 B:255 (White)"
    selected_color_box_color = Color(255, 255, 255, 255)

    while not object_layer_render.window_should_close():
        current_time = time.time()
        delta_time = current_time - last_frame_time
        last_frame_time = current_time

        # --- Input Handling ---

        # Handle mode switching (Edit Mode / Normal Mode)
        for i in range(10):  # Keys 0-9 for Edit Mode
            if object_layer_render.is_key_pressed(KEY_ZERO + i):
                if not is_edit_mode:  # Only clone if entering edit mode
                    is_edit_mode = True
                    if current_object_layer_id not in cloned_object_layer_states:
                        # Deep copy original data and ensure colors are Color objects
                        original_render_data_for_clone = copy.deepcopy(
                            original_object_layer_data[current_object_layer_id][
                                "RENDER_DATA"
                            ]
                        )
                        processed_colors = []
                        for item in original_render_data_for_clone["COLORS"]:
                            if isinstance(item, (tuple, list)) and len(item) == 4:
                                processed_colors.append(
                                    Color(item[0], item[1], item[2], item[3])
                                )
                            elif isinstance(item, Color):
                                processed_colors.append(item)
                            else:
                                logging.warning(
                                    f"Unexpected color data type during clone creation: {type(item)}. Skipping."
                                )
                        original_render_data_for_clone["COLORS"] = processed_colors

                        cloned_object_layer_states[current_object_layer_id] = {
                            "RENDER_DATA": original_render_data_for_clone,
                            "SEED_DATA": copy.deepcopy(
                                original_object_layer_data[current_object_layer_id][
                                    "SEED_DATA"
                                ]
                            ),
                            "MODIFIED": False,
                        }
                        selected_color_rgb_text = "Selected: R:255 G:255 B:255 (White)"
                        selected_color_box_color = Color(255, 255, 255, 255)

                    # Update animation instance to use clone's data (which now has Color objects)
                    update_animation_instance_data_source()  # Call helper here
                demo_object_layer_animation_instance.pause_at_frame(i, current_time)
                # Ensure the animation instance is paused and using the correct frame
                demo_object_layer_animation_instance.is_paused = (
                    True  # Explicitly set pause state
                )
                demo_object_layer_animation_instance.paused_frame_index = (
                    i  # Set the paused frame
                )
                demo_object_layer_animation_instance.current_frame_index = (
                    i  # Update current frame index
                )
                demo_object_layer_animation_instance.frame_timer = 0.0  # Reset timer

        if object_layer_render.is_key_pressed(KEY_ENTER):
            is_edit_mode = False
            demo_object_layer_animation_instance.resume()
            update_animation_instance_data_source()  # Call helper here

        # Handle Backspace to reset current clone
        if object_layer_render.is_key_pressed(KEY_BACKSPACE):
            if current_object_layer_id in cloned_object_layer_states:
                logging.info(f"Resetting clone for {current_object_layer_id}")
                cloned_object_layer_states[current_object_layer_id] = {
                    "RENDER_DATA": copy.deepcopy(
                        original_object_layer_data[current_object_layer_id][
                            "RENDER_DATA"
                        ]
                    ),
                    "SEED_DATA": copy.deepcopy(
                        original_object_layer_data[current_object_layer_id]["SEED_DATA"]
                    ),
                    "MODIFIED": False,
                }
                # Process colors to Color objects after deepcopy for the reset clone
                reset_clone_render_data = cloned_object_layer_states[
                    current_object_layer_id
                ]["RENDER_DATA"]
                processed_colors_on_reset = []
                for item in reset_clone_render_data["COLORS"]:
                    if isinstance(item, (tuple, list)) and len(item) == 4:
                        processed_colors_on_reset.append(
                            Color(item[0], item[1], item[2], item[3])
                        )
                    elif isinstance(item, Color):
                        processed_colors_on_reset.append(item)
                    else:
                        logging.warning(
                            f"Unexpected color data type during clone reset: {type(item)}. Skipping."
                        )
                reset_clone_render_data["COLORS"] = processed_colors_on_reset

                update_animation_instance_data_source()  # Call helper here
                # If in edit mode, re-pause at the current frame to refresh display
                if is_edit_mode:
                    demo_object_layer_animation_instance.pause_at_frame(
                        demo_object_layer_animation_instance.paused_frame_index,
                        current_time,
                    )
                # Reset selected color when clone is explicitly reset
                selected_color_rgb_text = "Selected: R:255 G:255 B:255 (White)"
                selected_color_box_color = Color(255, 255, 255, 255)
            else:
                logging.info(f"No clone to reset for {current_object_layer_id}")

        # Handle 'T' key for Test Mode toggle
        if object_layer_render.is_key_pressed(KEY_T):
            is_test_mode = not is_test_mode
            logging.info(f"Test Mode toggled: {'ON' if is_test_mode else 'OFF'}")
            if not is_edit_mode:  # Only update data source if not in edit mode
                update_animation_instance_data_source()

        # Handle direction and mode changes (applies in both Normal and Edit modes)
        # Note: In Edit Mode, this changes which frame of the CLONE is displayed,
        # but does not affect the pixel data of the CLONE itself.
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
        else:  # For stateless animations
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
        if object_layer_render.is_key_pressed(KEY_W):
            current_pixel_size_in_display += 1.0
            object_layer_render.get_or_create_object_layer_animation(
                demo_obj_id,
                current_object_layer_id,
                current_pixel_size_in_display,
                initial_direction=demo_object_layer_animation_instance.current_direction,
            )
        elif object_layer_render.is_key_pressed(KEY_Q):
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
        if object_layer_render.is_key_pressed(KEY_E):
            current_object_layer_id_index = (current_object_layer_id_index + 1) % len(
                AVAILABLE_OBJECT_LAYER_IDS
            )
            current_object_layer_id = AVAILABLE_OBJECT_LAYER_IDS[
                current_object_layer_id_index
            ]
            object_layer_render.remove_object_layer_animation(
                obj_id=demo_obj_id, object_layer_id=None
            )
            object_layer_mode = ObjectLayerMode.IDLE
            last_commanded_dx = 0.0
            last_commanded_dy = 0.0
            is_edit_mode = False  # Exit edit mode on ID change

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
            update_animation_instance_data_source()  # Call helper here

        elif object_layer_render.is_key_pressed(KEY_R):
            current_object_layer_id_index = (
                current_object_layer_id_index - 1 + len(AVAILABLE_OBJECT_LAYER_IDS)
            ) % len(AVAILABLE_OBJECT_LAYER_IDS)
            current_object_layer_id = AVAILABLE_OBJECT_LAYER_IDS[
                current_object_layer_id_index
            ]
            object_layer_render.remove_object_layer_animation(
                obj_id=demo_obj_id, object_layer_id=None
            )
            object_layer_mode = ObjectLayerMode.IDLE
            last_commanded_dx = 0.0
            last_commanded_dy = 0.0
            is_edit_mode = False  # Exit edit mode on ID change

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
            update_animation_instance_data_source()  # Call helper here

        # Get current animation instance properties (after potential ID change)
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

        # Draw grid and palettes only if in edit mode
        if is_edit_mode:
            # Get the current frame matrix to determine its dimensions
            frame_matrix, _, _, _ = (
                demo_object_layer_animation_instance.get_current_frame_data(
                    current_time
                )
            )
            if frame_matrix:
                matrix_rows = len(frame_matrix)
                matrix_cols = len(frame_matrix[0]) if matrix_rows > 0 else 0

                grid_color = Color(200, 200, 200, 100)  # Light gray with transparency

                # Draw vertical lines
                for col_idx in range(matrix_cols + 1):
                    start_x = draw_x + col_idx * current_pixel_size_in_display
                    object_layer_render.draw_line(
                        int(start_x),
                        int(draw_y),
                        int(start_x),
                        int(draw_y + total_rendered_height),
                        grid_color,
                    )
                # Draw horizontal lines
                for row_idx in range(matrix_rows + 1):
                    start_y = draw_y + row_idx * current_pixel_size_in_display
                    object_layer_render.draw_line(
                        int(draw_x),
                        int(start_y),
                        int(draw_x + total_rendered_width),
                        int(start_y),
                        grid_color,
                    )

            # Calculate positions for palettes at bottom-right
            total_palette_width = PALETTE_COLS_RAINBOW * PALETTE_SQUARE_SIZE

            # Rainbow palette position
            rainbow_palette_start_x = (
                SCREEN_WIDTH - total_palette_width - PALETTE_PADDING
            )
            rainbow_palette_start_y = (
                SCREEN_HEIGHT
                - (PALETTE_ROWS_RAINBOW * PALETTE_SQUARE_SIZE * 4)  # Space for 4 rows
                - (PALETTE_PADDING * 4)  # Padding between rows
            )

            # Draw the 16-color rainbow palette
            for i, color in enumerate(COLOR_PALETTE_RAINBOW):
                col = i % PALETTE_COLS_RAINBOW
                row = i // PALETTE_COLS_RAINBOW

                square_x = rainbow_palette_start_x + col * PALETTE_SQUARE_SIZE
                square_y = rainbow_palette_start_y + row * PALETTE_SQUARE_SIZE

                object_layer_render.draw_rectangle(
                    square_x, square_y, PALETTE_SQUARE_SIZE, PALETTE_SQUARE_SIZE, color
                )
                object_layer_render.draw_rectangle(
                    square_x,
                    square_y,
                    PALETTE_SQUARE_SIZE,
                    PALETTE_SQUARE_SIZE,
                    Color(255, 255, 255, 50),  # Semi-transparent white border
                )

            # Draw the first darker rainbow palette
            darker_rainbow_palette_1_y = (
                rainbow_palette_start_y + PALETTE_SQUARE_SIZE + PALETTE_PADDING
            )
            for i, color in enumerate(COLOR_PALETTE_RAINBOW_DARKER_1):
                col = i % PALETTE_COLS_RAINBOW
                row = i // PALETTE_COLS_RAINBOW

                square_x = rainbow_palette_start_x + col * PALETTE_SQUARE_SIZE
                square_y = darker_rainbow_palette_1_y + row * PALETTE_SQUARE_SIZE

                object_layer_render.draw_rectangle(
                    square_x, square_y, PALETTE_SQUARE_SIZE, PALETTE_SQUARE_SIZE, color
                )
                object_layer_render.draw_rectangle(
                    square_x,
                    square_y,
                    PALETTE_SQUARE_SIZE,
                    PALETTE_SQUARE_SIZE,
                    Color(255, 255, 255, 50),
                )

            # Draw the second darker rainbow palette
            darker_rainbow_palette_2_y = (
                darker_rainbow_palette_1_y + PALETTE_SQUARE_SIZE + PALETTE_PADDING
            )
            for i, color in enumerate(COLOR_PALETTE_RAINBOW_DARKER_2):
                col = i % PALETTE_COLS_RAINBOW
                row = i // PALETTE_COLS_RAINBOW

                square_x = rainbow_palette_start_x + col * PALETTE_SQUARE_SIZE
                square_y = darker_rainbow_palette_2_y + row * PALETTE_SQUARE_SIZE

                object_layer_render.draw_rectangle(
                    square_x, square_y, PALETTE_SQUARE_SIZE, PALETTE_SQUARE_SIZE, color
                )
                object_layer_render.draw_rectangle(
                    square_x,
                    square_y,
                    PALETTE_SQUARE_SIZE,
                    PALETTE_SQUARE_SIZE,
                    Color(255, 255, 255, 50),
                )

            # Lightness bar position (below all rainbow palettes)
            lightness_bar_start_x = SCREEN_WIDTH - total_palette_width - PALETTE_PADDING
            lightness_bar_start_y = (
                darker_rainbow_palette_2_y + PALETTE_SQUARE_SIZE + PALETTE_PADDING
            )

            # Draw the lightness/darkness bar
            for i, color in enumerate(COLOR_PALETTE_LIGHTNESS):
                square_x = lightness_bar_start_x + i * PALETTE_SQUARE_SIZE
                square_y = lightness_bar_start_y

                object_layer_render.draw_rectangle(
                    square_x, square_y, PALETTE_SQUARE_SIZE, PALETTE_SQUARE_SIZE, color
                )
                object_layer_render.draw_rectangle(
                    square_x,
                    square_y,
                    PALETTE_SQUARE_SIZE,
                    PALETTE_SQUARE_SIZE,
                    Color(255, 255, 255, 50),
                )

            # Eyedropper logic: Detect clicks on palettes
            if object_layer_render.is_mouse_button_pressed(MOUSE_BUTTON_LEFT):
                mouse_pos = object_layer_render.get_mouse_position()
                mouse_x, mouse_y = mouse_pos.x, mouse_pos.y

                clicked_on_palette = False
                temp_selected_color = None

                # Check original rainbow palette click
                if (
                    rainbow_palette_start_x
                    <= mouse_x
                    < rainbow_palette_start_x + total_palette_width
                    and rainbow_palette_start_y
                    <= mouse_y
                    < rainbow_palette_start_y
                    + (PALETTE_ROWS_RAINBOW * PALETTE_SQUARE_SIZE)
                ):
                    clicked_col = int(
                        (mouse_x - rainbow_palette_start_x) // PALETTE_SQUARE_SIZE
                    )
                    clicked_row = int(
                        (mouse_y - rainbow_palette_start_y) // PALETTE_SQUARE_SIZE
                    )
                    index = clicked_row * PALETTE_COLS_RAINBOW + clicked_col
                    if 0 <= index < len(COLOR_PALETTE_RAINBOW):
                        temp_selected_color = COLOR_PALETTE_RAINBOW[index]
                        clicked_on_palette = True

                # Check first darker rainbow palette click
                elif (
                    rainbow_palette_start_x
                    <= mouse_x
                    < rainbow_palette_start_x + total_palette_width
                    and darker_rainbow_palette_1_y
                    <= mouse_y
                    < darker_rainbow_palette_1_y
                    + (PALETTE_ROWS_RAINBOW * PALETTE_SQUARE_SIZE)
                ):
                    clicked_col = int(
                        (mouse_x - rainbow_palette_start_x) // PALETTE_SQUARE_SIZE
                    )
                    clicked_row = int(
                        (mouse_y - darker_rainbow_palette_1_y) // PALETTE_SQUARE_SIZE
                    )
                    index = clicked_row * PALETTE_COLS_RAINBOW + clicked_col
                    if 0 <= index < len(COLOR_PALETTE_RAINBOW_DARKER_1):
                        temp_selected_color = COLOR_PALETTE_RAINBOW_DARKER_1[index]
                        clicked_on_palette = True

                # Check second darker rainbow palette click
                elif (
                    rainbow_palette_start_x
                    <= mouse_x
                    < rainbow_palette_start_x + total_palette_width
                    and darker_rainbow_palette_2_y
                    <= mouse_y
                    < darker_rainbow_palette_2_y
                    + (PALETTE_ROWS_RAINBOW * PALETTE_SQUARE_SIZE)
                ):
                    clicked_col = int(
                        (mouse_x - rainbow_palette_start_x) // PALETTE_SQUARE_SIZE
                    )
                    clicked_row = int(
                        (mouse_y - darker_rainbow_palette_2_y) // PALETTE_SQUARE_SIZE
                    )
                    index = clicked_row * PALETTE_COLS_RAINBOW + clicked_col
                    if 0 <= index < len(COLOR_PALETTE_RAINBOW_DARKER_2):
                        temp_selected_color = COLOR_PALETTE_RAINBOW_DARKER_2[index]
                        clicked_on_palette = True

                # Check lightness palette click
                elif (
                    lightness_bar_start_x
                    <= mouse_x
                    < lightness_bar_start_x + total_palette_width
                    and lightness_bar_start_y
                    <= mouse_y
                    < lightness_bar_start_y
                    + (PALETTE_ROWS_LIGHTNESS * PALETTE_SQUARE_SIZE)
                ):
                    clicked_col = int(
                        (mouse_x - lightness_bar_start_x) // PALETTE_SQUARE_SIZE
                    )
                    clicked_row = int(
                        (mouse_y - lightness_bar_start_y) // PALETTE_SQUARE_SIZE
                    )
                    index = clicked_row * PALETTE_COLS_LIGHTNESS + clicked_col
                    if 0 <= index < len(COLOR_PALETTE_LIGHTNESS):
                        temp_selected_color = COLOR_PALETTE_LIGHTNESS[index]
                        clicked_on_palette = True

                if clicked_on_palette and temp_selected_color:
                    selected_color_rgb_text = f"Selected: R:{temp_selected_color.r} G:{temp_selected_color.g} B:{temp_selected_color.b}"
                    selected_color_box_color = temp_selected_color

                # --- Paint functionality on the frame grid ---
                # Check if click is on the rendered object's grid
                if (
                    draw_x <= mouse_x < draw_x + total_rendered_width
                    and draw_y <= mouse_y < draw_y + total_rendered_height
                    and selected_color_box_color  # Only paint if a color is selected
                ):
                    clicked_col_on_grid = int(
                        (mouse_x - draw_x) // current_pixel_size_in_display
                    )
                    clicked_row_on_grid = int(
                        (mouse_y - draw_y) // current_pixel_size_in_display
                    )

                    current_frame_matrix = demo_object_layer_animation_instance.get_current_frame_matrix_for_editing(
                        current_time
                    )
                    current_color_map = (
                        demo_object_layer_animation_instance.get_current_color_map_for_editing()
                    )

                    # Find index of selected_color_box_color in current_color_map
                    color_index = -1
                    for idx, color in enumerate(current_color_map):
                        if (
                            color.r == selected_color_box_color.r
                            and color.g == selected_color_box_color.g
                            and color.b == selected_color_box_color.b
                            and color.a == selected_color_box_color.a
                        ):
                            color_index = idx
                            break

                    if color_index == -1:  # Color not found, add it to the map
                        current_color_map.append(selected_color_box_color)
                        color_index = len(current_color_map) - 1
                        # IMPORTANT: Also update the clone's RENDER_DATA["COLORS"] with the new Color object
                        cloned_object_layer_states[current_object_layer_id][
                            "RENDER_DATA"
                        ]["COLORS"].append(selected_color_box_color)

                    # Ensure bounds
                    if 0 <= clicked_row_on_grid < len(
                        current_frame_matrix
                    ) and 0 <= clicked_col_on_grid < len(current_frame_matrix[0]):

                        # Apply color to the clone's frame data
                        # Note: current_frame_matrix is already a reference to the clone's data
                        # if we are in edit mode and a clone exists.
                        current_frame_matrix[clicked_row_on_grid][
                            clicked_col_on_grid
                        ] = color_index
                        cloned_object_layer_states[current_object_layer_id][
                            "MODIFIED"
                        ] = True
                        logging.info(
                            f"Painted cell ({clicked_row_on_grid}, {clicked_col_on_grid}) with color index {color_index}"
                        )

            # Display selected color RGB text and box (only in edit mode)
            object_layer_render.draw_text(
                selected_color_rgb_text,
                UI_START_X,  # Use UI_START_X for consistent left alignment
                current_y,
                SUB_LINE_HEIGHT,
                Color(255, 255, 255, 255),
            )
            # Calculate position for the color box next to the text
            text_width = object_layer_render.measure_text(
                selected_color_rgb_text, SUB_LINE_HEIGHT
            )
            color_box_x = UI_START_X + text_width + 10  # 10 pixels padding
            color_box_size = 20  # Small box size
            object_layer_render.draw_rectangle(
                color_box_x,
                current_y,
                color_box_size,
                color_box_size,
                selected_color_box_color,
            )
            current_y += SUB_LINE_HEIGHT + PADDING

        # --- UI Text Display (Top-Left) ---
        UI_START_X = 10
        UI_START_Y = 10
        LINE_HEIGHT = 20  # For main titles
        SUB_LINE_HEIGHT = 15  # For smaller info
        PADDING = 5

        current_y = UI_START_Y

        object_layer_render.draw_text(
            f"Current Object Layer ID: {current_object_layer_id}",
            UI_START_X,
            current_y,
            LINE_HEIGHT,
            Color(255, 255, 255, 255),
        )
        current_y += LINE_HEIGHT + PADDING

        # Object State: Original / Clone (Modified) / Clone (Unmodified)
        object_state_text = "Original"
        if is_edit_mode:
            if (
                current_object_layer_id in cloned_object_layer_states
                and cloned_object_layer_states[current_object_layer_id]["MODIFIED"]
            ):
                object_state_text = "Clone (Modified)"
            else:
                object_state_text = "Clone (Unmodified)"
        object_layer_render.draw_text(
            f"Object State: {object_state_text}",
            UI_START_X,
            current_y,
            SUB_LINE_HEIGHT,
            (
                Color(255, 255, 0, 255)
                if "Clone" in object_state_text
                else Color(200, 200, 200, 255)
            ),
        )
        current_y += SUB_LINE_HEIGHT + PADDING

        if demo_object_layer_animation_instance.is_stateless:
            object_layer_render.draw_text(
                "Stateless Animation (Direction & Mode Ignored)",
                UI_START_X,
                current_y,
                SUB_LINE_HEIGHT,
                Color(200, 200, 200, 255),
            )
            current_y += SUB_LINE_HEIGHT + PADDING
        else:
            object_layer_render.draw_text(
                f"Direction: {demo_object_layer_animation_instance.current_direction.name}",
                UI_START_X,
                current_y,
                LINE_HEIGHT,
                Color(255, 255, 255, 255),
            )
            current_y += LINE_HEIGHT
            object_layer_render.draw_text(
                f"Mode: {demo_object_layer_animation_instance.object_layer_mode.name.capitalize()}",
                UI_START_X,
                current_y,
                SUB_LINE_HEIGHT,
                Color(200, 200, 200, 255),
            )
            current_y += SUB_LINE_HEIGHT
            object_layer_render.draw_text(
                "Press SPACE to toggle mode",
                UI_START_X,
                current_y,
                SUB_LINE_HEIGHT,
                Color(200, 200, 200, 255),
            )
            current_y += SUB_LINE_HEIGHT
            object_layer_render.draw_text(
                "Use numpad/arrow keys to set direction",
                UI_START_X,
                current_y,
                SUB_LINE_HEIGHT,
                Color(200, 200, 200, 255),
            )
            current_y += SUB_LINE_HEIGHT + PADDING

        object_layer_render.draw_text(
            f"Pixel Size: {current_pixel_size_in_display:.2f}",
            UI_START_X,
            current_y,
            SUB_LINE_HEIGHT,
            Color(200, 200, 200, 255),
        )
        current_y += SUB_LINE_HEIGHT
        object_layer_render.draw_text(
            "Use 'Q' for zoom out and 'W' for zoom in",
            UI_START_X,
            current_y,
            SUB_LINE_HEIGHT,
            Color(200, 200, 200, 255),
        )
        current_y += SUB_LINE_HEIGHT
        object_layer_render.draw_text(
            "Use 'E'/'R' to change object layer ID",
            UI_START_X,
            current_y,
            SUB_LINE_HEIGHT,
            Color(200, 200, 200, 255),
        )
        current_y += SUB_LINE_HEIGHT + PADDING

        object_layer_render.draw_text(
            f"Frame Index: {demo_object_layer_animation_instance.current_frame_index}",
            UI_START_X,
            current_y,
            SUB_LINE_HEIGHT,
            Color(200, 200, 200, 255),
        )
        current_y += SUB_LINE_HEIGHT
        object_layer_render.draw_text(
            f"Frame Timer: {demo_object_layer_animation_instance.frame_timer:.2f}",
            UI_START_X,
            current_y,
            SUB_LINE_HEIGHT,
            Color(200, 200, 200, 255),
        )
        current_y += SUB_LINE_HEIGHT

        # Display mode status
        if is_edit_mode:
            object_layer_render.draw_text(
                f"Mode: EDIT MODE (Frame {demo_object_layer_animation_instance.paused_frame_index})",
                UI_START_X,
                current_y,
                SUB_LINE_HEIGHT,
                Color(255, 0, 0, 255),
            )
            current_y += SUB_LINE_HEIGHT
            object_layer_render.draw_text(
                "Click on grid to paint. Press ENTER to return to Normal Mode.",
                UI_START_X,
                current_y,
                SUB_LINE_HEIGHT,
                Color(255, 255, 255, 255),
            )
            current_y += SUB_LINE_HEIGHT
            object_layer_render.draw_text(
                "Press BACKSPACE to reset clone.",
                UI_START_X,
                current_y,
                SUB_LINE_HEIGHT,
                Color(255, 255, 255, 255),
            )
            current_y += SUB_LINE_HEIGHT + PADDING
        else:
            object_layer_render.draw_text(
                "Mode: NORMAL MODE",
                UI_START_X,
                current_y,
                SUB_LINE_HEIGHT,
                Color(0, 255, 0, 255),
            )
            current_y += SUB_LINE_HEIGHT
            object_layer_render.draw_text(
                "Press 0-9 to enter Edit Mode",
                UI_START_X,
                current_y,
                SUB_LINE_HEIGHT,
                Color(200, 200, 200, 255),
            )
            current_y += SUB_LINE_HEIGHT
            object_layer_render.draw_text(
                f"Test Mode: {'ON' if is_test_mode else 'OFF'} (Press 'T' to toggle)",
                UI_START_X,
                current_y,
                SUB_LINE_HEIGHT,
                Color(255, 165, 0, 255),  # Orange color for test mode
            )
            current_y += SUB_LINE_HEIGHT + PADDING

        # FPS display at bottom left (fixed position)
        object_layer_render.draw_text(
            f"FPS: {int(1.0 / object_layer_render.get_frame_time()) if object_layer_render.get_frame_time() > 0 else 'N/A'}",
            UI_START_X,
            SCREEN_HEIGHT - 30,  # Fixed at bottom
            20,
            Color(255, 255, 255, 255),
        )

        object_layer_render.end_drawing()

    object_layer_render.close_window()
