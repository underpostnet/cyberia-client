import collections
import logging
import math
import time
from enum import Enum, auto

from raylibpy import (
    BLACK,
    LIGHTGRAY,
    Color,
    Vector2,
    begin_drawing,
    begin_mode2d,
    clear_background,
    close_window,
    draw_circle,
    draw_line,
    draw_rectangle,
    draw_text,
    end_drawing,
    end_mode2d,
    get_frame_time,
    get_mouse_position,
    get_screen_to_world2d,
    init_window,
    is_key_pressed,
    is_mouse_button_pressed,
    set_target_fps,
    window_should_close,
    Camera2D,
    measure_text as raylib_measure_text,
)

from config import CAMERA_SMOOTHNESS, DIRECTION_HISTORY_LENGTH
from object_layer.object_layer_data import ObjectLayerMode, Direction
from network_state.network_object import NetworkObject

logging.basicConfig(
    level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s"
)


class ObjectLayerAnimation:
    """Manages the animation state for a single object layer, including frames, colors, and playback."""

    def __init__(
        self,
        frames_map: dict,
        raw_color_map: list[tuple[int, int, int, int]],
        frame_duration: float = 0.15,
        is_stateless: bool = False,
    ):
        self.frames_map = frames_map
        # Convert raw color tuples to raylibpy.Color objects upon initialization
        self.color_map = [Color(r, g, b, a) for r, g, b, a in raw_color_map]
        self.frame_duration = frame_duration
        self.is_stateless = is_stateless

        self.current_frame_index = 0
        self.frame_timer = 0.0
        self.last_moving_timestamp = 0.0

        self.current_direction = Direction.DOWN
        self.object_layer_mode = ObjectLayerMode.IDLE

        self.is_paused = False
        self.paused_frame_index = 0

    def set_state(self, direction: Direction, mode: ObjectLayerMode, timestamp: float):
        """Sets the current direction and mode of the animation.
        For stateless animations, direction and mode are overridden to NONE_IDLE.
        """
        new_direction = direction
        new_mode = mode

        if self.is_stateless:
            new_direction = Direction.NONE
            new_mode = ObjectLayerMode.IDLE

        state_changed = (self.current_direction != new_direction) or (
            self.object_layer_mode != new_mode
        )

        self.current_direction = new_direction
        self.object_layer_mode = new_mode

        if state_changed and not self.is_paused:
            self.current_frame_index = 0
            self.frame_timer = 0.0

        if self.object_layer_mode == ObjectLayerMode.WALKING:
            self.last_moving_timestamp = timestamp

    def _get_current_object_layer_key(self, timestamp: float) -> str:
        """Determines the appropriate animation key based on current state (direction and mode)."""
        if self.is_stateless:
            return "NONE_IDLE" if "NONE_IDLE" in self.frames_map else "DEFAULT_IDLE"

        direction_prefix = self.current_direction.name
        object_layer_suffix = self.object_layer_mode.name

        object_layer_key = f"{direction_prefix}_{object_layer_suffix}"

        # Fallback logic if specific animation key is not found
        if object_layer_key not in self.frames_map:
            fallback_key = f"{direction_prefix}_IDLE"
            if fallback_key in self.frames_map:
                return fallback_key
            else:
                return "DEFAULT_IDLE"  # Ultimate fallback

        return object_layer_key

    def update(self, dt: float, timestamp: float):
        """Updates the animation frame based on delta time, advancing the animation."""
        if self.is_paused:
            return

        self.frame_timer += dt

        object_layer_key = self._get_current_object_layer_key(timestamp)
        frames_list = self.frames_map.get(object_layer_key)

        if not frames_list:  # Ensure there's at least a default frame
            frames_list = [[[0]]]

        num_frames = len(frames_list)
        if num_frames == 0:
            return

        # Advance frame if frame duration is met
        if self.frame_timer >= self.frame_duration:
            self.current_frame_index = (self.current_frame_index + 1) % num_frames
            self.frame_timer = 0.0

        # If only one frame, ensure it stays at index 0
        if num_frames == 1:
            self.current_frame_index = 0
            self.frame_timer = 0.0

    def set_frames_and_colors(self, new_frames_map: dict, new_colors_data: list):
        """Dynamically updates the animation's frames and color map.
        Ensures self.color_map always contains Color objects, converting from tuples/lists if necessary.
        """
        self.frames_map = new_frames_map

        processed_color_map = []
        for item in new_colors_data:
            if (
                isinstance(item, (tuple, list)) and len(item) == 4
            ):  # Handle both tuple and list of 4 ints (r,g,b,a)
                processed_color_map.append(Color(item[0], item[1], item[2], item[3]))
            elif isinstance(
                item, Color
            ):  # If it's already a Color object, use it directly
                processed_color_map.append(item)
            else:
                logging.warning(f"Unexpected color data type: {type(item)}. Skipping.")
        self.color_map = processed_color_map

        # Reset animation state to frame 0 to reflect new data immediately
        self.current_frame_index = 0
        self.frame_timer = 0.0

    def get_current_frame_matrix_for_editing(self, timestamp: float) -> list[list[int]]:
        """Returns the current frame matrix for direct modification (e.g., painting)."""
        object_layer_key = self._get_current_object_layer_key(timestamp)
        frames_list = self.frames_map.get(object_layer_key)

        if not frames_list:
            return [[0]]  # Return a minimal valid matrix if no frames

        current_frame_index_to_use = self.current_frame_index
        if self.is_paused:
            current_frame_index_to_use = self.paused_frame_index
            if current_frame_index_to_use >= len(
                frames_list
            ):  # Ensure paused frame index is valid
                current_frame_index_to_use = 0

        # Return a reference to the actual frame data for direct modification
        return frames_list[current_frame_index_to_use]

    def get_current_color_map_for_editing(self) -> list[Color]:
        """Returns the current color map for direct modification (e.g., adding colors)."""
        return self.color_map

    def get_current_frame_data(
        self, timestamp: float
    ) -> tuple[list[list[int]], list[Color], bool, int]:
        """Returns the current frame matrix, color map, horizontal flip status (always False), and current frame index."""
        object_layer_key = self._get_current_object_layer_key(timestamp)
        frames_list = self.frames_map.get(object_layer_key)

        if not frames_list:
            return [[[0]]], self.color_map, False, 0  # Default if no frames

        current_frame_index_to_use = self.current_frame_index
        if self.is_paused:
            current_frame_index_to_use = self.paused_frame_index
            if current_frame_index_to_use >= len(
                frames_list
            ):  # Ensure paused frame index is valid
                current_frame_index_to_use = 0

        current_frame_matrix = frames_list[current_frame_index_to_use]

        # Removed horizontal flip logic. Always return False for flip_horizontal.
        flip_horizontal = False

        return (
            current_frame_matrix,
            self.color_map,
            flip_horizontal,
            current_frame_index_to_use,
        )

    def pause_at_frame(self, frame_index: int, timestamp: float):
        """Pauses the animation at a specific frame."""
        object_layer_key = self._get_current_object_layer_key(timestamp)
        frames_list = self.frames_map.get(object_layer_key)

        if not frames_list:
            logging.warning(
                f"Cannot pause, no frames found for key: {object_layer_key}"
            )
            self.is_paused = True
            self.paused_frame_index = 0
            return

        if 0 <= frame_index < len(frames_list):
            self.is_paused = True
            self.paused_frame_index = frame_index
            self.current_frame_index = frame_index
            self.frame_timer = 0.0
            logging.info(f"Animation paused at frame {frame_index}")
        else:
            logging.warning(
                f"Invalid frame index {frame_index} for pausing. Max frames: {len(frames_list)}"
            )
            self.is_paused = (
                True  # Still pause, but at current frame if index is invalid
            )
            self.paused_frame_index = self.current_frame_index

    def resume(self):
        """Resumes the animation from its current frame."""
        if self.is_paused:
            self.is_paused = False
            self.frame_timer = 0.0
            logging.info("Animation resumed.")


class ObjectLayerRender:
    """Handles all rendering operations using Raylib, including drawing network objects and managing their animations."""

    def __init__(
        self,
        screen_width: int,
        screen_height: int,
        world_width: int,
        world_height: int,
        network_object_size: int,
        object_layer_data: dict,
        title: str = "Object Layer Application",
        target_fps: int = 60,
    ):
        self.screen_width = screen_width
        self.screen_height = screen_height
        self.world_width = world_width
        self.world_height = world_height
        self.network_object_size = network_object_size
        self.title = title
        set_target_fps(target_fps)

        init_window(self.screen_width, self.screen_height, self.title)

        self.camera = Camera2D()
        self.camera.offset = Vector2(self.screen_width / 2, self.screen_height / 2)
        self.camera.rotation = 0.0
        self.camera.zoom = 1.0
        self.camera.target = Vector2(0, 0)

        self._object_layer_animation_instances: dict[str, dict] = {}
        self._object_layer_direction_histories: dict[
            tuple[str, str], collections.deque[Direction]
        ] = {}
        self._rendered_network_object_positions: dict[str, Vector2] = {}
        self.object_layer_data = object_layer_data

    def begin_drawing(self):
        """Starts the Raylib drawing phase."""
        begin_drawing()

    def end_drawing(self):
        """Ends the Raylib drawing phase."""
        end_drawing()

    def clear_background(self, color: Color):
        """Clears the background with a specified color."""
        clear_background(color)

    def begin_camera_mode(self):
        """Begins 2D camera mode for drawing world elements."""
        begin_mode2d(self.camera)

    def end_camera_mode(self):
        """Ends 2D camera mode."""
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

    def measure_text(self, text: str, font_size: int) -> int:
        """Measures the width of the given text for a specific font size."""
        return raylib_measure_text(text, font_size)

    def get_frame_time(self) -> float:
        """Returns the time elapsed since the last frame."""
        return get_frame_time()

    def get_mouse_position(self) -> Vector2:
        """Returns the current mouse cursor position in screen coordinates."""
        return get_mouse_position()

    def get_world_mouse_position(self) -> Vector2:
        """Returns the current mouse cursor position in world coordinates (considering camera)."""
        return get_screen_to_world2d(get_mouse_position(), self.camera)

    def is_mouse_button_pressed(self, button: int) -> bool:
        """Checks if a mouse button has been pressed in the current frame."""
        return is_mouse_button_pressed(button)

    def is_key_pressed(self, key: int) -> bool:
        """Checks if a keyboard key has been pressed in the current frame."""
        return is_key_pressed(key)

    def window_should_close(self) -> bool:
        """Checks if the window close button has been pressed or ESC key is pressed."""
        return window_should_close()

    def close_window(self):
        """Closes the Raylib window."""
        close_window()

    def update_camera_target(self, target_world_pos: Vector2, smoothness: float = 1.0):
        """Smoothly moves the camera target towards a world position."""
        self.camera.target.x += (target_world_pos.x - self.camera.target.x) * smoothness
        self.camera.target.y += (target_world_pos.y - self.camera.target.y) * smoothness

    def set_camera_zoom(self, zoom_factor: float):
        """Sets the camera zoom level."""
        self.camera.zoom = zoom_factor

    def get_camera_zoom(self) -> float:
        """Returns the current camera zoom level."""
        return self.camera.zoom

    def draw_grid(self):
        """Draws a grid over the world based on network object size."""
        for x in range(0, self.world_width + 1, self.network_object_size):
            self.draw_line(x, 0, x, self.world_height, LIGHTGRAY)
        for y in range(0, self.world_height + 1, self.network_object_size):
            self.draw_line(0, y, self.world_width, y, LIGHTGRAY)

    def get_object_layer_data_for_id(self, object_layer_id: str) -> dict | None:
        """Retrieves animation data for a specific object layer ID from the loaded data."""
        object_data = self.object_layer_data.get(object_layer_id)
        return object_data.get("RENDER_DATA") if object_data else None

    def get_object_layer_matrix_dimension(self, object_layer_id: str) -> int:
        """Determines the dimension (e.g., 8 for an 8x8 sprite) of the animation matrix for a given object layer ID."""
        object_layer_info = self.get_object_layer_data_for_id(object_layer_id)
        if not object_layer_info:
            logging.warning(
                f"No object layer data found for {object_layer_id}. Cannot determine dimensions."
            )
            return 1  # Default to 1x1 if data is missing

        first_frame = None
        # Try to find a default or any available frame to determine dimension
        for key in ["DEFAULT_IDLE", "NONE_IDLE"] + list(
            object_layer_info["FRAMES"].keys()
        ):
            frames_list = object_layer_info["FRAMES"].get(key)
            if frames_list and len(frames_list) > 0:
                first_frame = frames_list[0]
                break

        if first_frame and len(first_frame) > 0:
            return len(first_frame)
        else:
            logging.warning(
                f"Could not determine matrix dimension for {object_layer_id}. Returning 1."
            )
            return 1

    def draw_network_object(
        self,
        network_object: NetworkObject,
        current_timestamp: float,
        current_dx: float,
        current_dy: float,
    ):
        """Draws a network object, handling its animation based on its type and movement."""
        object_layer_ids_to_use = network_object.object_layer_ids

        target_world_pos = Vector2(network_object.x, network_object.y)

        # Initialize or update smoothed position for rendering
        if network_object.obj_id not in self._rendered_network_object_positions:
            self._rendered_network_object_positions[network_object.obj_id] = (
                target_world_pos
            )

        smoothed_pos = self._rendered_network_object_positions[network_object.obj_id]

        # Apply smoothing to the object's rendered position
        smoothed_pos.x += (target_world_pos.x - smoothed_pos.x) * CAMERA_SMOOTHNESS
        smoothed_pos.y += (target_world_pos.y - smoothed_pos.y) * CAMERA_SMOOTHNESS

        self._rendered_network_object_positions[network_object.obj_id] = smoothed_pos

        draw_x_base = smoothed_pos.x
        draw_y_base = smoothed_pos.y

        # If no specific object layers are defined, draw a fallback rectangle
        if not object_layer_ids_to_use:
            self.draw_rectangle(
                int(draw_x_base),
                int(draw_y_base),
                self.network_object_size,
                self.network_object_size,
                BLACK,
            )
            self.draw_rectangle(
                int(draw_x_base) + 2,
                int(draw_y_base) + 2,
                self.network_object_size - 4,
                self.network_object_size - 4,
                network_object.color,
            )
            return

        # Determine object layer mode (e.g., IDLE or WALKING)
        object_layer_mode = ObjectLayerMode.IDLE
        if network_object.path and network_object.path_index < len(network_object.path):
            object_layer_mode = ObjectLayerMode.WALKING

        # Iterate and draw each associated object layer
        for object_layer_id in object_layer_ids_to_use:
            object_layer_info = self.get_object_layer_data_for_id(object_layer_id)
            if not object_layer_info:
                logging.warning(
                    f"No object layer info found for ID: {object_layer_id}. Skipping rendering for {network_object.obj_id}."
                )
                continue

            # Calculate the size of each pixel in the display based on network object size and matrix dimension
            matrix_dimension = self.get_object_layer_matrix_dimension(object_layer_id)
            target_object_layer_size_pixels = (
                self.network_object_size / matrix_dimension
            )
            if target_object_layer_size_pixels == 0:
                target_object_layer_size_pixels = 1

            # Get or create the animation instance for this specific object layer
            self.get_or_create_object_layer_animation(
                obj_id=network_object.obj_id,
                object_layer_id=object_layer_id,
                target_object_layer_size_pixels=target_object_layer_size_pixels,
                initial_direction=Direction.DOWN,  # Default initial direction
            )

            # Update direction and mode for non-stateless animations
            if not object_layer_info["IS_STATELESS"]:
                self.update_object_layer_direction_for_object(
                    obj_id=network_object.obj_id,
                    object_layer_id=object_layer_id,
                    current_dx=current_dx,
                    current_dy=current_dy,
                    object_layer_mode=object_layer_mode,
                    timestamp=current_timestamp,
                )
            else:  # For stateless animations, set a default state
                anim_properties = self.get_object_layer_animation_properties(
                    network_object.obj_id, object_layer_id
                )
                if anim_properties:
                    anim_instance = anim_properties["object_layer_animation_instance"]
                    anim_instance.set_state(
                        Direction.NONE, ObjectLayerMode.IDLE, current_timestamp
                    )

            draw_x = draw_x_base
            draw_y = draw_y_base

            # Render the animation frame
            self.render_object_layer_animation(
                obj_id=network_object.obj_id,
                object_layer_id=object_layer_id,
                screen_x=draw_x,
                screen_y=draw_y,
                timestamp=current_timestamp,
            )

    def _render_object_layer_frame(
        self,
        frame_matrix: list[list[int]],
        color_map: list[Color],
        screen_x: float,
        screen_y: float,
        pixel_size_in_display: float,
    ):
        """Renders a single frame of an object layer animation pixel by pixel."""
        matrix_dimension = len(frame_matrix)
        if matrix_dimension == 0:
            return

        # Round values for drawing to avoid pixel gaps due to floating point inaccuracies
        rounded_screen_x = round(screen_x)
        rounded_screen_y = round(screen_y)
        rounded_pixel_size = round(pixel_size_in_display)
        if rounded_pixel_size == 0:
            rounded_pixel_size = 1

        for row_idx in range(matrix_dimension):
            for col_idx in range(matrix_dimension):
                matrix_value = frame_matrix[row_idx][col_idx]

                # Ensure matrix_value is a valid index for color_map
                if not (0 <= matrix_value < len(color_map)):
                    logging.warning(
                        f"Matrix value {matrix_value} is out of bounds for color_map (size {len(color_map)}). Defaulting to black."
                    )
                    current_color = BLACK  # Fallback to black if index is out of bounds
                else:
                    current_color = color_map[matrix_value]

                if current_color.a == 0:  # Skip fully transparent pixels
                    continue
                cell_draw_x = rounded_screen_x + col_idx * rounded_pixel_size
                cell_draw_y = rounded_screen_y + row_idx * rounded_pixel_size

                self.draw_rectangle(
                    int(cell_draw_x),
                    int(cell_draw_y),
                    rounded_pixel_size,
                    rounded_pixel_size,
                    current_color,
                )

    def get_or_create_object_layer_animation(
        self,
        obj_id: str,
        object_layer_id: str,
        target_object_layer_size_pixels: float,
        initial_direction: Direction,
    ) -> dict:
        """Retrieves an existing ObjectLayerAnimation instance or creates a new one if it doesn't exist."""
        anim_key = f"{obj_id}_{object_layer_id}"
        current_cached_props = self._object_layer_animation_instances.get(anim_key)

        if current_cached_props:
            # Update pixel size if it has changed
            if (
                current_cached_props["target_object_layer_size_pixels"]
                != target_object_layer_size_pixels
            ):
                current_cached_props["target_object_layer_size_pixels"] = (
                    target_object_layer_size_pixels
                )
            return current_cached_props

        object_layer_info = self.get_object_layer_data_for_id(object_layer_id)
        if not object_layer_info:
            raise ValueError(
                f"No object layer data found for object layer ID '{object_layer_id}'."
            )

        # Create a new animation instance
        anim_instance = ObjectLayerAnimation(
            frames_map=object_layer_info["FRAMES"],
            raw_color_map=object_layer_info["COLORS"],
            frame_duration=object_layer_info["FRAME_DURATION"],
            is_stateless=object_layer_info["IS_STATELESS"],
        )
        self._object_layer_animation_instances[anim_key] = {
            "object_layer_animation_instance": anim_instance,
            "target_object_layer_size_pixels": target_object_layer_size_pixels,
            "object_layer_id": object_layer_id,
        }

        # Manage direction history for non-stateless animations
        if not object_layer_info["IS_STATELESS"]:
            history_key = (obj_id, object_layer_id)
            if history_key not in self._object_layer_direction_histories:
                self._object_layer_direction_histories[history_key] = collections.deque(
                    maxlen=DIRECTION_HISTORY_LENGTH
                )
                self._object_layer_direction_histories[history_key].append(
                    initial_direction
                )
        else:
            # Remove history for stateless animations if it exists
            history_key = (obj_id, object_layer_id)
            if history_key in self._object_layer_direction_histories:
                del self._object_layer_direction_histories[history_key]

        return self._object_layer_animation_instances[anim_key]

    def remove_object_layer_animation(
        self, obj_id: str, object_layer_id: str | None = None
    ):
        """Removes an object layer animation instance and its associated direction history."""
        keys_to_remove = []
        if object_layer_id:  # Remove a specific object layer animation
            anim_key = f"{obj_id}_{object_layer_id}"
            if anim_key in self._object_layer_animation_instances:
                keys_to_remove.append((anim_key, (obj_id, object_layer_id)))
        else:  # Remove all object layer animations for a given object ID
            for key in list(self._object_layer_animation_instances.keys()):
                if key.startswith(f"{obj_id}_"):
                    parts = key.split(f"{obj_id}_", 1)
                    if len(parts) > 1:
                        keys_to_remove.append((key, (obj_id, parts[1])))
                    else:
                        keys_to_remove.append(
                            (key, (obj_id, ""))
                        )  # Handle cases where ID might be empty

        for anim_key, history_key_tuple in keys_to_remove:
            if anim_key in self._object_layer_animation_instances:
                del self._object_layer_animation_instances[anim_key]
            if history_key_tuple in self._object_layer_direction_histories:
                del self._object_layer_direction_histories[history_key_tuple]

        # Also remove the smoothed position for the network object
        if obj_id in self._rendered_network_object_positions:
            del self._rendered_network_object_positions[obj_id]

    def reset_smoothed_object_position(self, obj_id: str):
        """Resets the smoothed rendering position of a network object."""
        if obj_id in self._rendered_network_object_positions:
            del self._rendered_network_object_positions[obj_id]

    def update_object_layer_direction_for_object(
        self,
        obj_id: str,
        object_layer_id: str,
        current_dx: float,
        current_dy: float,
        object_layer_mode: ObjectLayerMode,
        timestamp: float,
        reverse: bool = False,  # Flag to reverse direction for viewer display
    ):
        """Updates the direction and mode of an object's animation based on its movement."""
        history_key = (obj_id, object_layer_id)
        direction_history = self._object_layer_direction_histories.get(history_key)

        if not direction_history:
            return

        movement_threshold = 1.0  # Minimum movement to consider a direction change
        current_movement_magnitude = math.sqrt(current_dx**2 + current_dy**2)

        if current_movement_magnitude > movement_threshold:
            # Normalize movement components to -1, 0, or 1
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

            # Map normalized movement to a Direction enum
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
            # Special handling for viewer demo to reverse directions if needed
            if not reverse:
                direction_map = {
                    (0, -1): Direction.DOWN,
                    (1, -1): Direction.DOWN_LEFT,
                    (1, 0): Direction.LEFT,
                    (1, 1): Direction.UP_LEFT,
                    (0, 1): Direction.UP,
                    (-1, 1): Direction.UP_RIGHT,
                    (-1, 0): Direction.RIGHT,
                    (-1, -1): Direction.DOWN_RIGHT,
                }

            instantaneous_direction = direction_map.get(
                (norm_dx, norm_dy), Direction.DOWN  # Default to DOWN if no match
            )
            direction_history.append(instantaneous_direction)
        else:
            # If not moving, gradually reduce history to favor idle direction
            if len(direction_history) > 1:
                direction_history.popleft()

        # Determine smoothed direction from history (most common direction)
        smoothed_direction = Direction.DOWN
        if direction_history:
            from collections import Counter

            most_common_direction_tuple = Counter(direction_history).most_common(1)
            if most_common_direction_tuple:
                smoothed_direction = most_common_direction_tuple[0][0]
        else:
            if object_layer_mode == ObjectLayerMode.IDLE:
                smoothed_direction = Direction.DOWN

        # Update the animation instance with the new state
        anim_key = f"{obj_id}_{object_layer_id}"
        anim_properties = self._object_layer_animation_instances.get(anim_key)
        if anim_properties:
            anim_instance = anim_properties["object_layer_animation_instance"]
            anim_instance.set_state(smoothed_direction, object_layer_mode, timestamp)

    def update_all_active_object_layer_animations(
        self, delta_time: float, current_timestamp: float
    ):
        """Updates all currently active object layer animations."""
        for anim_properties in self._object_layer_animation_instances.values():
            anim_instance = anim_properties["object_layer_animation_instance"]
            anim_instance.update(dt=delta_time, timestamp=current_timestamp)

    def get_object_layer_animation_properties(
        self, obj_id: str, object_layer_id: str
    ) -> dict | None:
        """Retrieves properties (including the animation instance) of an active object layer animation."""
        anim_key = f"{obj_id}_{object_layer_id}"
        return self._object_layer_animation_instances.get(anim_key)

    def render_object_layer_animation(
        self,
        obj_id: str,
        object_layer_id: str,
        screen_x: float,
        screen_y: float,
        timestamp: float,
    ):
        """Renders a specific object layer animation at given screen coordinates."""
        anim_properties = self.get_object_layer_animation_properties(
            obj_id, object_layer_id
        )
        if not anim_properties:
            return

        anim_instance = anim_properties["object_layer_animation_instance"]
        target_pixel_size = anim_properties["target_object_layer_size_pixels"]

        # Get current frame data from the animation instance
        # Note: flip_horizontal is no longer returned or used.
        frame_matrix, color_map, _, _ = anim_instance.get_current_frame_data(timestamp)

        # Call the internal rendering method without flip_horizontal
        self._render_object_layer_frame(
            frame_matrix=frame_matrix,
            color_map=color_map,
            screen_x=screen_x,
            screen_y=screen_y,
            pixel_size_in_display=target_pixel_size,
        )

    def get_smoothed_object_position(self, obj_id: str) -> Vector2 | None:
        """Returns the smoothed rendering position of a network object."""
        return self._rendered_network_object_positions.get(obj_id)
