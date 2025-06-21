import logging
from pyray import Color, Rectangle, Vector2

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
        icon_texture=None,
        title_text: str = "",  # New: Default title for the modal
    ):
        """
        Initializes the ModalCoreComponent.

        Args:
            screen_width: The width of the game screen.
            screen_height: The height of the game screen.
            render_content_callback: A callable that takes (modal_component, object_layer_render_instance, x, y, width, height, data_to_pass)
                                     and renders the modal's internal content.
            width: The width of the modal in pixels.
            height: The height of the modal in pixels.
            padding_bottom: Padding from the bottom of the screen.
            padding_right: Padding from the right of the screen.
            horizontal_offset: Horizontal offset for positioning multiple modals.
            background_color: The background color of the modal.
            icon_texture: The pyray Texture object for the icon, if any.
            title_text: The initial title text for the modal.
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
        self.icon_texture = icon_texture  # Store the icon texture
        self.is_hovered = False  # State for hover effect
        self.is_clicked = False  # New state for click detection
        self.data_to_pass = {}  # New attribute to pass additional data to the callback
        self.title_text = title_text  # Store the title text

        # New attributes for maximization
        self.is_maximized: bool = False
        self.original_width: int = width
        self.original_height: int = height  # Store original height
        self.original_padding_right: int = padding_right
        self.original_padding_bottom: int = (
            padding_bottom  # Store original padding bottom
        )

        # Position the modal at the bottom-right with specified padding and offset
        self.x = (
            self.screen_width - self.width - self.padding_right - self.horizontal_offset
        )
        self.y = self.screen_height - self.height - self.padding_bottom

    def set_title(self, new_title: str):
        """Sets the title text for the modal."""
        self.title_text = new_title

    def set_maximized_state(
        self,
        is_maximized: bool,
        new_width: int = None,
        new_padding_right: int = None,
        new_height: int = None,
        new_padding_bottom: int = None,
    ):
        """
        Sets the maximized state of the modal and adjusts its dimensions and position.
        When maximizing, it stores current dimensions. When restoring, it uses stored dimensions.
        """
        if is_maximized:
            # Store original dimensions only if not already maximized
            if not self.is_maximized:
                self.original_width = self.width
                self.original_height = self.height
                self.original_padding_right = self.padding_right
                self.original_padding_bottom = self.padding_bottom

            self.is_maximized = True
            # Maximize to full available width/height, potentially adjusting padding_right and padding_bottom
            self.width = new_width if new_width is not None else self.screen_width
            self.height = new_height if new_height is not None else self.screen_height
            self.padding_right = (
                new_padding_right if new_padding_right is not None else 0
            )
            self.padding_bottom = (
                new_padding_bottom if new_padding_bottom is not None else 0
            )
            self.x = 0  # Maximize to left edge
            self.y = 0  # Maximize to top edge
        else:
            self.is_maximized = False
            # Restore original dimensions
            self.width = self.original_width
            self.height = self.original_height
            self.padding_right = self.original_padding_right
            self.padding_bottom = self.original_padding_bottom
            # Reposition based on original settings
            self.x = (
                self.screen_width
                - self.width
                - self.padding_right
                - self.horizontal_offset
            )
            self.y = self.screen_height - self.height - self.padding_bottom

        logging.info(
            f"Modal maximized state set to {self.is_maximized}. New dimensions: {self.width}x{self.height} at ({self.x},{self.y})"
        )

    def check_click(
        self, mouse_x: int, mouse_y: int, is_mouse_button_pressed: bool
    ) -> bool:
        """
        Checks if the modal was clicked in the current frame.
        Updates hover state and sets is_clicked if a click occurred within its bounds.

        Args:
            mouse_x: Current X coordinate of the mouse.
            mouse_y: Current Y coordinate of the mouse.
            is_mouse_button_pressed: Boolean indicating if the primary mouse button is pressed.

        Returns:
            True if the modal was clicked in this frame, False otherwise.
        """
        # Update hover state
        self.is_hovered = (
            mouse_x >= self.x
            and mouse_x <= (self.x + self.width)
            and mouse_y >= self.y
            and mouse_y <= (self.y + self.height)
        )

        # Update click state
        self.is_clicked = self.is_hovered and is_mouse_button_pressed
        return self.is_clicked

    def render(
        self,
        object_layer_render_instance,
        mouse_x: int = -1,
        mouse_y: int = -1,
        is_mouse_button_down: bool = False,
    ):
        """
        Renders the modal's background and then calls the content rendering callback.
        Now includes mouse coordinates to determine hover state and passes additional data.

        Args:
            object_layer_render_instance: An instance of ObjectLayerRender
                                          used for drawing operations.
            mouse_x: Current X coordinate of the mouse.
            mouse_y: Current Y coordinate of the mouse.
            is_mouse_button_down: True if the mouse button is currently held down.
        """
        # Update hover state (redundant if check_click is called first, but kept for standalone render calls)
        self.is_hovered = (
            mouse_x >= self.x
            and mouse_x <= (self.x + self.width)
            and mouse_y >= self.y
            and mouse_y <= (self.y + self.height)
        )

        # Draw the modal background
        object_layer_render_instance.draw_rectangle(
            self.x, self.y, self.width, self.height, self.background_color
        )

        # Pass the current title text to the render_content_callback
        self.data_to_pass["title_text"] = self.title_text

        # Pass maximized state to content rendering callback
        self.data_to_pass["is_maximized"] = self.is_maximized

        # Pass mouse button down state
        self.data_to_pass["is_mouse_button_down"] = is_mouse_button_down

        # Call the external callback to render content within the modal's bounds
        self.render_content_callback(
            self,  # Pass the modal component instance itself
            object_layer_render_instance,
            self.x,
            self.y,
            self.width,
            self.height,
            self.data_to_pass,  # Pass the additional data
        )
