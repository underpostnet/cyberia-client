import logging

from raylibpy import (
    Color,
    Vector2,
    Camera2D,
    RAYWHITE,
    DARKGRAY,
    LIGHTGRAY,
    BLACK,
    BLUE,
    RED,
    GREEN,
    MOUSE_BUTTON_LEFT,
    init_window,
    set_target_fps,
    begin_drawing,
    clear_background,
    begin_mode2d,
    draw_line,
    draw_rectangle,
    draw_circle,
    draw_text,
    end_mode2d,
    get_frame_time,
    get_mouse_position,
    get_screen_to_world2d,
    close_window,
    window_should_close,
    is_mouse_button_pressed,
    end_drawing,
)

# --- Logging Configuration ---
logging.basicConfig(
    level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s"
)

from config import (
    CAMERA_SMOOTHNESS,
)

from instance_object import InstanceObject


class InstanceRenderer:
    """
    Handles all Raylib rendering logic, including camera control.
    """

    def __init__(
        self,
        screen_width: int,
        screen_height: int,
        world_width: int,
        world_height: int,
        object_size: int,
    ):
        self.screen_width = screen_width
        self.screen_height = screen_height
        self.world_width = world_width
        self.world_height = world_height
        self.object_size = object_size

        init_window(screen_width, screen_height, "Python MMO Instance Client")
        set_target_fps(60)

        self.camera = Camera2D()
        self.camera.offset = Vector2(screen_width / 2, screen_height / 2)
        self.camera.rotation = 0.0
        self.camera.zoom = 1.0
        self.camera.target = Vector2(0, 0)  # Initial target

        self.camera_target_world = Vector2(
            0, 0
        )  # The desired world position for the camera to smoothly move to

    def begin_frame(self):
        """Starts drawing for a new frame."""
        begin_drawing()
        clear_background(RAYWHITE)
        begin_mode2d(self.camera)

    def end_frame(self):
        """Ends drawing for the current frame."""
        end_mode2d()
        end_drawing()

    def draw_grid(self):
        """Draws the 1600x1600 grid lines."""
        for x in range(0, self.world_width + 1, self.object_size):
            draw_line(x, 0, x, self.world_height, LIGHTGRAY)
        for y in range(0, self.world_height + 1, self.object_size):
            draw_line(0, y, self.world_width, y, LIGHTGRAY)

    def draw_object(self, obj: InstanceObject):
        """Draws an instance object."""
        draw_rectangle(
            int(obj.x), int(obj.y), self.object_size, self.object_size, obj.color
        )
        # Draw outline for obstacles
        if obj.is_obstacle:
            draw_rectangle(
                int(obj.x), int(obj.y), self.object_size, self.object_size, BLACK
            )  # Fill with black
            draw_rectangle(
                int(obj.x) + 2,
                int(obj.y) + 2,
                self.object_size - 4,
                self.object_size - 4,
                obj.color,
            )  # Inner color
        else:
            draw_rectangle(
                int(obj.x), int(obj.y), self.object_size, self.object_size, obj.color
            )

    def draw_path(self, path: list[dict[str, float]]):
        """Draws the path for the player."""
        if len(path) < 2:
            return
        for i in range(len(path) - 1):
            start_point = path[i]
            end_point = path[i + 1]
            draw_line(
                int(start_point["X"]),
                int(start_point["Y"]),
                int(end_point["X"]),
                int(end_point["Y"]),
                GREEN,
            )
            draw_circle(
                int(start_point["X"]), int(start_point["Y"]), 5, GREEN
            )  # Mark path points
        draw_circle(int(path[-1]["X"]), int(path[-1]["Y"]), 5, RED)  # End point

    def draw_debug_info(self, text: str, x: int, y: int, font_size: int, color: Color):
        """Draws debug text on the screen."""
        draw_text(text, x, y, font_size, color)

    def update_camera(self, target_world_pos: Vector2):
        """Smoothly moves the camera towards the target world position."""
        # Lerp the camera target towards the desired position
        self.camera_target_world.x = (
            self.camera_target_world.x
            + (target_world_pos.x - self.camera_target_world.x) * CAMERA_SMOOTHNESS
        )
        self.camera_target_world.y = (
            self.camera_target_world.y
            + (target_world_pos.y - self.camera_target_world.y) * CAMERA_SMOOTHNESS
        )

        # Set the camera's actual target
        self.camera.target = self.camera_target_world

    def get_world_mouse_position(self) -> Vector2:
        """Converts mouse screen position to world position."""
        return get_screen_to_world2d(get_mouse_position(), self.camera)

    def window_should_close(self) -> bool:
        """Checks if the window should close."""
        return window_should_close()

    def close_window(self):
        """Closes the Raylib window."""
        close_window()
