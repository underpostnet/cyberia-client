import logging
from raylibpy import Camera2D, Vector2, get_screen_width, get_screen_height

from config import CAMERA_SMOOTHNESS, NETWORK_OBJECT_SIZE, WORLD_WIDTH, WORLD_HEIGHT


logging.basicConfig(
    level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s"
)


class CameraManager:
    """
    Manages the Raylib 2D camera, including its target, zoom, and handling
    of window resizing to maintain a responsive view.
    """

    def __init__(self, initial_screen_width: int, initial_screen_height: int):
        """
        Initializes the CameraManager with the initial screen dimensions.

        Args:
            initial_screen_width: The initial width of the game window.
            initial_screen_height: The initial height of the game window.
        """
        self.camera = Camera2D()
        self.camera.offset = Vector2(
            initial_screen_width / 2, initial_screen_height / 2
        )
        self.camera.rotation = 0.0
        self.camera.zoom = 1.0
        self.camera.target = Vector2(0, 0)  # Initial target at world origin

        self.screen_width = initial_screen_width
        self.screen_height = initial_screen_height

    def update_camera_target(
        self, target_world_pos: Vector2, smoothness: float = CAMERA_SMOOTHNESS
    ):
        """
        Smoothly moves the camera target towards a specified world position.

        Args:
            target_world_pos: The target world coordinates for the camera.
            smoothness: A float value (0.0 to 1.0) controlling the camera's follow speed.
        """
        self.camera.target.x += (target_world_pos.x - self.camera.target.x) * smoothness
        self.camera.target.y += (target_world_pos.y - self.camera.target.y) * smoothness

    def set_camera_zoom(self, zoom_factor: float):
        """
        Sets the camera zoom level.

        Args:
            zoom_factor: The desired zoom level. 1.0 is no zoom.
        """
        self.camera.zoom = zoom_factor

    def get_camera_zoom(self) -> float:
        """
        Returns the current camera zoom level.
        """
        return self.camera.zoom

    def handle_window_resize(self):
        """
        Updates the camera's offset and internal screen dimensions when the window is resized.
        This ensures the camera remains centered relative to the new window size.
        """
        new_screen_width = get_screen_width()
        new_screen_height = get_screen_height()

        if (
            self.screen_width != new_screen_width
            or self.screen_height != new_screen_height
        ):
            logging.info(
                f"Window resized from ({self.screen_width}x{self.screen_height}) to ({new_screen_width}x{new_screen_height})"
            )
            self.screen_width = new_screen_width
            self.screen_height = new_screen_height
            # Recalculate camera offset to keep it centered
            self.camera.offset = Vector2(self.screen_width / 2, self.screen_height / 2)

            # Optionally, adjust zoom based on new aspect ratio or size if needed
            # For now, we'll keep zoom constant, but this is where more complex
            # responsive scaling logic would go.
