import logging
from raylibpy import Color, RAYWHITE, Vector2  # Import Vector2 for slot positioning
from config import UI_FONT_SIZE, UI_TEXT_COLOR_PRIMARY, UI_TEXT_COLOR_SHADING
from object_layer.object_layer_render import ObjectLayerRender

logging.basicConfig(
    level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s"
)


class BagCyberiaSlot:
    """
    Represents a single slot within the Cyberia Bag UI.
    This class handles the rendering of an individual inventory slot,
    which can potentially hold an item.
    """

    def __init__(self, x: int, y: int, width: int, height: int):
        """
        Initializes a BagCyberiaSlot.

        Args:
            x: The X coordinate of the top-left corner of the slot.
            y: The Y coordinate of the top-left corner of the slot.
            width: The width of the slot.
            height: The height of the slot.
        """
        self.x = x
        self.y = y
        self.width = width
        self.height = height
        self.background_color = Color(
            50, 50, 50, 200
        )  # Darker grey for slot background
        self.border_color = Color(100, 100, 100, 255)  # Light grey for slot border

    def render(self, object_layer_render_instance: ObjectLayerRender):
        """
        Renders the individual bag slot.

        Args:
            object_layer_render_instance: An instance of ObjectLayerRender used for drawing operations.
        """
        # Draw slot background
        object_layer_render_instance.draw_rectangle(
            self.x, self.y, self.width, self.height, self.background_color
        )
        # Draw slot border (slightly larger rectangle then inner filled rectangle)
        object_layer_render_instance.draw_rectangle(
            self.x, self.y, self.width, self.height, self.border_color
        )
        object_layer_render_instance.draw_rectangle(
            self.x + 1,
            self.y + 1,
            self.width - 2,
            self.height - 2,
            self.background_color,
        )

        # Placeholder for future item rendering within the slot
        # if self.item:
        #     self.item.render(object_layer_render_instance, self.x, self.y, self.width, self.height)


class BagCyberiaView:
    """
    A view responsible for rendering the content of the Cyberia Bag modal.
    This class encapsulates the UI elements and their drawing logic for the bag inventory.
    It now manages a grid of BagCyberiaSlot entities.
    """

    SLOT_SIZE = 40  # Each slot will be 40px x 40px
    SLOT_PADDING = 10  # Padding between slots

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
        This now includes a grid of inventory slots.

        Args:
            object_layer_render_instance: An instance of ObjectLayerRender used for drawing operations.
            x: The X coordinate of the top-left corner of the modal's content area.
            y: The Y coordinate of the top-left corner of the modal's content area.
            width: The width of the modal's content area.
            height: The height of the modal's content area.
        """
        padding_around_grid = 20
        current_y = y + padding_around_grid

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
        current_y += (UI_FONT_SIZE + 4) + padding_around_grid  # Space after title

        # Calculate grid dimensions and starting position
        num_rows = 5
        num_cols = 6

        grid_width = (BagCyberiaView.SLOT_SIZE * num_cols) + (
            BagCyberiaView.SLOT_PADDING * (num_cols - 1)
        )
        grid_height = (BagCyberiaView.SLOT_SIZE * num_rows) + (
            BagCyberiaView.SLOT_PADDING * (num_rows - 1)
        )

        # Center the grid within the available content area
        grid_start_x = x + (width - grid_width) // 2
        grid_start_y = current_y

        # Render the grid of slots
        for row in range(num_rows):
            for col in range(num_cols):
                slot_x = grid_start_x + col * (
                    BagCyberiaView.SLOT_SIZE + BagCyberiaView.SLOT_PADDING
                )
                slot_y = grid_start_y + row * (
                    BagCyberiaView.SLOT_SIZE + BagCyberiaView.SLOT_PADDING
                )

                bag_slot = BagCyberiaSlot(
                    slot_x, slot_y, BagCyberiaView.SLOT_SIZE, BagCyberiaView.SLOT_SIZE
                )
                bag_slot.render(object_layer_render_instance)
