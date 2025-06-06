import logging
from raylibpy import Color, Vector2
from config import UI_FONT_SIZE, UI_TEXT_COLOR_PRIMARY, UI_TEXT_COLOR_SHADING
from object_layer.object_layer_render import ObjectLayerRender

logging.basicConfig(
    level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s"
)


class MapCyberiaView:
    """
    Manages the display and interaction for the map interface.
    This view renders a simple centered gray square as a placeholder for the map.
    """

    def __init__(self, object_layer_render_instance: ObjectLayerRender):
        self.object_layer_render = object_layer_render_instance
        self.title_text = "World Map"

    def render_content(
        self,
        modal_component,
        x: int,
        y: int,
        width: int,
        height: int,
        mouse_x: int,
        mouse_y: int,
        is_mouse_button_pressed: bool,
    ):
        """
        Renders the map view content: a centered gray square.

        Args:
            modal_component: The parent ModalCoreComponent instance.
            x, y, width, height: Dimensions of the modal container.
            mouse_x, mouse_y: Current mouse coordinates.
            is_mouse_button_pressed: True if the mouse button is pressed.
        """
        # Update modal title
        modal_component.set_title(self.title_text)

        # Draw the title for the map view
        title_text = self.title_text
        title_font_size = UI_FONT_SIZE + 2
        title_text_width = self.object_layer_render.measure_text(
            title_text, title_font_size
        )
        title_x = x + (width - title_text_width) // 2
        title_y = y + 20

        self.object_layer_render.draw_text(
            title_text,
            title_x + 1,
            title_y + 1,
            title_font_size,
            Color(*UI_TEXT_COLOR_SHADING),
        )
        self.object_layer_render.draw_text(
            title_text,
            title_x,
            title_y,
            title_font_size,
            Color(*UI_TEXT_COLOR_PRIMARY),
        )

        # Define the size of the square
        square_size = 200

        # Calculate coordinates to center the square within the modal's content area
        # Account for the title area when centering vertically
        content_area_y_start = y + title_font_size + 30
        content_area_height = height - (content_area_y_start - y)

        square_x = x + (width - square_size) // 2
        square_y = content_area_y_start + (content_area_height - square_size) // 2

        # Draw the centered gray square
        self.object_layer_render.draw_rectangle(
            square_x,
            square_y,
            square_size,
            square_size,
            Color(100, 100, 100, 255),  # Gray color
        )

        # Optionally draw a border for the square
        self.object_layer_render.draw_rectangle_lines(
            square_x,
            square_y,
            square_size,
            square_size,
            Color(50, 50, 50, 255),  # Darker gray border
        )

    def handle_item_clicks(
        self,
        offset_x: int,
        offset_y: int,
        container_width: int,
        container_height: int,
        mouse_x: int,
        mouse_y: int,
        is_mouse_button_pressed: bool,
    ) -> bool:
        """
        Handles clicks within the map modal.
        This view has no interactive elements, so it always returns False.
        """
        return False

    def reset_view(self):
        """
        Resets the view state. For the map view, there is no specific state to reset.
        """
        self.title_text = "World Map"  # Ensure title is reset if it were dynamic
        pass
