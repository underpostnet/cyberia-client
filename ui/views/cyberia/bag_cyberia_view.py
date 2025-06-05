import logging
from raylibpy import Color, RAYWHITE
from config import UI_FONT_SIZE, UI_TEXT_COLOR_PRIMARY, UI_TEXT_COLOR_SHADING
from object_layer.object_layer_render import ObjectLayerRender

logging.basicConfig(
    level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s"
)


class BagCyberiaView:
    """
    A view responsible for rendering the content of the Cyberia Bag modal.
    This class encapsulates the UI elements and their drawing logic for the bag inventory.
    """

    @staticmethod
    def render_content(
        object_layer_render_instance: ObjectLayerRender,
        x: int,
        y: int,
        width: int,
        height: int,
    ):
        """
        Renders the detailed content of the bag view within the given bounds.

        Args:
            object_layer_render_instance: An instance of ObjectLayerRender used for drawing operations.
            x: The X coordinate of the top-left corner of the modal's content area.
            y: The Y coordinate of the top-left corner of the modal's content area.
            width: The width of the modal's content area.
            height: The height of the modal's content area.
        """
        padding = 20
        item_spacing = 30
        current_y = y + padding

        # Title for the bag
        title_text = "Cyberia Bag"
        title_width = object_layer_render_instance.measure_text(
            title_text, UI_FONT_SIZE + 4
        )  # Slightly larger font for title
        title_x = x + (width - title_width) // 2
        object_layer_render_instance.draw_text(
            title_text,
            title_x + 1,
            current_y + 1,
            UI_FONT_SIZE + 4,
            Color(*UI_TEXT_COLOR_SHADING),
        )
        object_layer_render_instance.draw_text(
            title_text,
            title_x,
            current_y,
            UI_FONT_SIZE + 4,
            Color(*UI_TEXT_COLOR_PRIMARY),
        )
        current_y += item_spacing * 2  # Add more space after title

        # Placeholder for inventory items
        items = [
            "Data Chip (3)",
            "Energy Cell (5)",
            "Health Pack (1)",
            "Quest Item: Amulet",
        ]

        for item in items:
            # Draw shading for text
            object_layer_render_instance.draw_text(
                item,
                x + padding + 1,
                current_y + 1,
                UI_FONT_SIZE,
                Color(*UI_TEXT_COLOR_SHADING),
            )
            # Draw actual text
            object_layer_render_instance.draw_text(
                item,
                x + padding,
                current_y,
                UI_FONT_SIZE,
                Color(*UI_TEXT_COLOR_PRIMARY),
            )
            current_y += item_spacing

        # You can add more complex rendering here, e.g., item icons, descriptions, etc.
        # For demonstration, we're just drawing text.
