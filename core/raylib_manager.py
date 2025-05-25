import logging
from raylibpy import (
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
    Color,
    Vector2,
    Camera2D,
)

# --- Logging Configuration ---
logging.basicConfig(
    level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s"
)


class RaylibManager:
    """
    Centralized manager for all Raylib-py interactions.
    This class encapsulates window management, drawing primitives,
    input handling, and camera control, promoting a clean separation
    of concerns and making it easier to swap rendering backends.
    """

    def __init__(
        self,
        screen_width: int,
        screen_height: int,
        title: str = "Raylib Application",
        target_fps: int = 60,
    ):
        """
        Initializes the Raylib window and sets up the default camera.

        Args:
            screen_width (int): The width of the application window.
            screen_height (int): The height of the application window.
            title (str): The title of the application window.
            target_fps (int): The desired frames per second.
        """
        self.screen_width = screen_width
        self.screen_height = screen_height
        self.title = title
        self.target_fps = target_fps

        # Initialize Raylib window
        init_window(self.screen_width, self.screen_height, self.title)
        set_target_fps(self.target_fps)
        logging.info(
            f"Raylib window initialized: {self.screen_width}x{self.screen_height} @ {self.target_fps} FPS"
        )

        # Initialize default 2D camera
        self.camera = Camera2D()
        self.camera.offset = Vector2(self.screen_width / 2, self.screen_height / 2)
        self.camera.rotation = 0.0
        self.camera.zoom = 1.0
        self.camera.target = Vector2(0, 0)  # Initial target for the camera

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
        # Lerp the camera target towards the desired position
        self.camera.target.x += (target_world_pos.x - self.camera.target.x) * smoothness
        self.camera.target.y += (target_world_pos.y - self.camera.target.y) * smoothness

    def set_camera_zoom(self, zoom_factor: float):
        """Sets the camera's zoom level."""
        self.camera.zoom = zoom_factor

    def get_camera_zoom(self) -> float:
        """Returns the current camera zoom level."""
        return self.camera.zoom
