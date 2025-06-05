import logging
from raylibpy import Color, Rectangle

logging.basicConfig(
    level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s"
)


class ModalCoreComponent:
    """
    A UI component responsible for rendering a fixed-position modal overlay.
    It renders a transparent bar at a specified position on the screen.
    """

    def __init__(
        self,
        screen_width: int,
        screen_height: int,
        render_content_callback,
        width: int = 40,
        height: int = 40,
        padding_bottom: int = 5,
        padding_right: int = 5,
        horizontal_offset: int = 0,
        background_color: Color = Color(0, 0, 0, 150),
    ):
        """
        Initializes the ModalCoreComponent.

        Args:
            screen_width: The width of the game screen.
            screen_height: The height of the game screen.
            render_content_callback: A callable that takes (x, y, width, height)
                                     and renders the modal's internal content.
            width: The width of the modal in pixels.
            height: The height of the modal in pixels.
            padding_bottom: Padding from the bottom of the screen.
            padding_right: Padding from the right of the screen.
            horizontal_offset: Horizontal offset for positioning multiple modals.
            background_color: The background color of the modal.
        """
        self.screen_width = screen_width
        self.screen_height = screen_height
        self.render_content_callback = render_content_callback
        self.background_color = background_color
        self.width = width
        self.height = height
        self.padding_bottom = padding_bottom
        self.padding_right = padding_right
        self.horizontal_offset = horizontal_offset

        # Position the modal at the bottom-right with specified padding and offset
        self.x = (
            self.screen_width - self.width - self.padding_right - self.horizontal_offset
        )
        self.y = self.screen_height - self.height - self.padding_bottom

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
