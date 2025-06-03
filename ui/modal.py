import logging
from raylibpy import Color, Rectangle

logging.basicConfig(
    level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s"
)


class Modal:
    """
    A UI component responsible for rendering a modal overlay.
    Supports different modes, with 'top-bar' being the default,
    rendering a transparent bar at the top of the screen.
    """

    def __init__(
        self,
        screen_width: int,
        screen_height: int,
        render_content_callback,
        mode: str = "top-bar",
        height: int = 80,  # Default height for top-bar mode
        background_color: Color = Color(0, 0, 0, 150),  # Transparent black
    ):
        """
        Initializes the Modal.

        Args:
            screen_width: The width of the game screen.
            screen_height: The height of the game screen.
            render_content_callback: A callable that takes (x, y, width, height)
                                     and renders the modal's internal content.
            mode: The display mode of the modal (e.g., "top-bar").
            height: The height of the modal in pixels (primarily for "top-bar" mode).
            background_color: The background color of the modal.
        """
        self.screen_width = screen_width
        self.screen_height = screen_height
        self.render_content_callback = render_content_callback
        self.mode = mode
        self.background_color = background_color

        self.x = 0
        self.y = 0
        self.width = screen_width
        self.height = height  # Default height for top-bar

        self._configure_mode()

    def _configure_mode(self):
        """Configures modal dimensions and position based on the selected mode."""
        if self.mode == "top-bar":
            self.x = 0
            self.y = 0
            self.width = self.screen_width
            # Height is already set by default in __init__ for top-bar
        else:
            logging.warning(
                f"Unsupported modal mode: {self.mode}. Defaulting to top-bar behavior."
            )
            self.mode = "top-bar"
            self.x = 0
            self.y = 0
            self.width = self.screen_width
            # Height is already set by default in __init__ for top-bar

    def render(self, object_layer_render_instance):
        """
        Renders the modal's background and then calls the content rendering callback.

        Args:
            object_layer_render_instance: An instance of ObjectLayerRender
                                          used for drawing operations.
        """
        # Draw the modal background
        object_layer_render_instance.draw_rectangle(
            self.x, self.y, self.width, self.height, self.background_color
        )

        # Call the external callback to render content within the modal's bounds
        self.render_content_callback(
            object_layer_render_instance, self.x, self.y, self.width, self.height
        )
