import logging
from raylibpy import Color, RAYWHITE, Vector2
from config import (
    UI_FONT_SIZE,
    UI_TEXT_COLOR_PRIMARY,
    UI_TEXT_COLOR_SHADING,
)
from object_layer.object_layer_render import ObjectLayerRender
from ui.components.cyberia.modal_render_cyberia import (
    render_modal_object_layer_item_content,
)
from ui.components.core.modal_core_component import (
    ModalCoreComponent,
)  # Changed import back

logging.basicConfig(
    level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s"
)

# Explicitly set bag inventory constants
BAG_INVENTORY_ROWS = 5
BAG_INVENTORY_COLS = 6
BAG_SLOT_SIZE = 40
BAG_SLOT_PADDING = 10


class BagCyberiaSlot:
    """
    Represents a single slot within the Cyberia Bag UI.
    This class handles the rendering of an individual inventory slot,
    which can potentially hold an item, specifically an object layer "skin".
    """

    def __init__(
        self,
        x: int,
        y: int,
        width: int,
        height: int,
        object_layer_id_to_render: str = None,
    ):
        """
        Initializes a BagCyberiaSlot.

        Args:
            x: The X coordinate of the top-left corner of the slot.
            y: The Y coordinate of the top-left corner of the slot.
            width: The width of the slot.
            height: The height of the slot.
            object_layer_id_to_render: The ID of the object layer (e.g., "ANON", "AYLEEN")
                                       to render within this slot if it's a "skin" item.
        """
        self.x = x
        self.y = y
        self.width = width
        self.height = height
        self.background_color = Color(
            50, 50, 50, 200
        )  # Darker grey for slot background
        self.border_color = Color(100, 100, 100, 255)  # Light grey for slot border
        self.object_layer_id_to_render = (
            object_layer_id_to_render  # Store object layer ID
        )

    def render(self, object_layer_render_instance: ObjectLayerRender):
        """
        Renders the individual bag slot. If an object_layer_id_to_render is set,
        it delegates the rendering of that object layer's animation frame.

        Args:
            object_layer_render_instance: An instance of ObjectLayerRender used for drawing operations.
        """
        # Draw slot background
        object_layer_render_instance.draw_rectangle(
            self.x, self.y, self.width, self.height, self.background_color
        )
        # Draw slot border (slightly larger rectangle then inner filled rectangle)
        object_layer_render_instance.draw_rectangle(
            self.x + 1,
            self.y + 1,
            self.width - 2,
            self.height - 2,
            self.background_color,
        )

        if self.object_layer_id_to_render:
            # logging.info(
            #     f"BagCyberiaSlot rendering item: {self.object_layer_id_to_render} at ({self.x}, {self.y})"
            # )

            # Create a dummy object to mimic ModalCoreComponent for passing data
            # This allows render_modal_object_layer_item_content to access the ID
            dummy_modal = ModalCoreComponent(  # Changed to ModalCoreComponent
                screen_width=0,  # Not relevant for this specific usage
                screen_height=0,  # Not relevant for this specific usage
                render_content_callback=None,  # Will be overridden by the specific render function
                width=self.width,
                height=self.height,
            )
            # Assign the object_layer_id_to_render directly to the dummy_modal
            # This is specifically for render_modal_object_layer_item_content to access it
            dummy_modal.object_layer_id_to_render = self.object_layer_id_to_render

            render_modal_object_layer_item_content(
                dummy_modal,  # Pass the dummy component
                object_layer_render_instance,
                self.x,
                self.y,
                self.width,
                self.height,
            )
        else:
            logging.debug("BagCyberiaSlot rendering empty slot.")


class BagCyberiaView:
    """
    A view responsible for rendering the content of the Cyberia Bag modal.
    This class encapsulates the UI elements and their drawing logic for the bag inventory.
    It now manages a grid of BagCyberiaSlot entities and displays "skin" type object layers.
    """

    @staticmethod
    def render_content(
        object_layer_render_instance: ObjectLayerRender,
        x: int,
        y: int,
        width: int,
        height: int,
        player_object_layer_ids: list[
            str
        ],  # New argument to receive player's object layers
    ):
        """
        Renders the detailed content of the bag view within the given bounds.
        This now includes a grid of inventory slots, populating them with player's
        "skin" object layers.

        Args:
            object_layer_render_instance: An instance of ObjectLayerRender used for drawing operations.
            x: The X coordinate of the top-left corner of the modal's content area.
            y: The Y coordinate of the top-left corner of the modal's content area.
            width: The width of the modal's content area.
            height: The height of the modal's content area.
            player_object_layer_ids: A list of object layer IDs currently associated with the player.
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

        # logging.info(
        #     f"BagCyberiaView: Player object layer IDs received: {player_object_layer_ids}"
        # )

        # Filter "skin" object layers from the player's active layers
        skin_object_layer_ids = []
        for obj_layer_id in player_object_layer_ids:
            obj_def = object_layer_render_instance.get_object_layer_definition(
                obj_layer_id
            )
            if obj_def and obj_def.get("TYPE") == "skin":
                skin_object_layer_ids.append(obj_layer_id)

        # logging.info(
        #     f"BagCyberiaView: Filtered skin object layer IDs: {skin_object_layer_ids}"
        # )

        current_skin_index = 0  # To track which skin item to assign to a slot

        # Calculate grid dimensions and starting position
        num_rows = BAG_INVENTORY_ROWS
        num_cols = BAG_INVENTORY_COLS

        grid_width = (BAG_SLOT_SIZE * num_cols) + (BAG_SLOT_PADDING * (num_cols - 1))
        grid_height = (BAG_SLOT_SIZE * num_rows) + (BAG_SLOT_PADDING * (num_rows - 1))

        # Center the grid within the available content area
        grid_start_x = x + (width - grid_width) // 2
        grid_start_y = current_y

        # Render the grid of slots
        for row in range(num_rows):
            for col in range(num_cols):
                slot_x = grid_start_x + col * (BAG_SLOT_SIZE + BAG_SLOT_PADDING)
                slot_y = grid_start_y + row * (BAG_SLOT_SIZE + BAG_SLOT_PADDING)

                object_layer_id_for_slot = None
                if current_skin_index < len(skin_object_layer_ids):
                    object_layer_id_for_slot = skin_object_layer_ids[current_skin_index]
                    current_skin_index += 1

                bag_slot = BagCyberiaSlot(
                    slot_x,
                    slot_y,
                    BAG_SLOT_SIZE,
                    BAG_SLOT_SIZE,
                    object_layer_id_for_slot,  # Pass the object layer ID for rendering
                )
                bag_slot.render(object_layer_render_instance)
