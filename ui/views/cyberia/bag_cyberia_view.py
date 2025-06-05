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
from ui.components.core.modal_core_component import ModalCoreComponent

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
    which can potentially hold an item, specifically an layer "skin".
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
        self.border_color = Color(
            100, 100, 100, 255
        )  # Default light grey for slot border
        self.object_layer_id_to_render = (
            object_layer_id_to_render  # Store object layer ID
        )
        self.is_hovered = False  # Track hover state for this slot
        self.is_clicked = False  # Track click state for this slot

    def check_click(
        self, mouse_x: int, mouse_y: int, is_mouse_button_pressed: bool
    ) -> bool:
        """
        Checks if the slot was clicked in the current frame.
        Updates internal click state based on hover and mouse button press.

        Args:
            mouse_x: Current X coordinate of the mouse.
            mouse_y: Current Y coordinate of the mouse.
            is_mouse_button_pressed: Boolean indicating if the primary mouse button is pressed.

        Returns:
            True if the slot was clicked in this frame (regardless of content), False otherwise.
        """
        # Update hover state
        self.is_hovered = (
            mouse_x >= self.x
            and mouse_x <= (self.x + self.width)
            and mouse_y >= self.y
            and mouse_y <= (self.y + self.height)
        )
        # Click is detected if hovered AND mouse button is pressed
        self.is_clicked = self.is_hovered and is_mouse_button_pressed
        return self.is_clicked

    def render(
        self,
        object_layer_render_instance: ObjectLayerRender,
        mouse_x: int,
        mouse_y: int,
    ):
        """
        Renders the individual bag slot. If an object_layer_id_to_render is set,
        it delegates the rendering of that object layer's animation frame.
        Updates hover state before rendering.

        Args:
            object_layer_render_instance: An instance of ObjectLayerRender used for drawing operations.
            mouse_x: Current X coordinate of the mouse.
            mouse_y: Current Y coordinate of the mouse.
        """
        # Update hover state directly in the render method
        self.is_hovered = (
            mouse_x >= self.x
            and mouse_x <= (self.x + self.width)
            and mouse_y >= self.y
            and mouse_y <= (self.y + self.height)
        )

        # Determine border color based on hover state or if it's clicked
        current_border_color = self.border_color
        if self.is_hovered:
            current_border_color = Color(*UI_TEXT_COLOR_PRIMARY)
        if self.is_clicked:  # Highlight if clicked (clicked state overrides hover)
            current_border_color = Color(255, 255, 0, 255)  # Yellow for clicked

        # Draw slot background
        object_layer_render_instance.draw_rectangle(
            self.x, self.y, self.width, self.height, self.background_color
        )
        # Draw slot border (slightly larger rectangle then inner filled rectangle)
        object_layer_render_instance.draw_rectangle(
            self.x, self.y, self.width, self.height, current_border_color
        )
        object_layer_render_instance.draw_rectangle(
            self.x + 1,
            self.y + 1,
            self.width - 2,
            self.height - 2,
            self.background_color,
        )

        if self.object_layer_id_to_render:
            # Create a dummy object to mimic ModalCoreComponent for passing data
            # This allows render_modal_object_layer_item_content to access the ID
            dummy_modal = ModalCoreComponent(
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

    def __init__(self, object_layer_render_instance: ObjectLayerRender):
        self.object_layer_render = object_layer_render_instance
        self.selected_object_layer_id: str | None = None
        self.title_text: str = "Bag"
        self.bag_slots: list[BagCyberiaSlot] = (
            []
        )  # List to hold persistent slot objects
        self._initialized_grid = False  # Flag to ensure grid initialization only once

    def _initialize_grid(self, x: int, y: int, width: int, height: int):
        """Initializes the grid of BagCyberiaSlot objects."""
        if self._initialized_grid:
            return

        padding_around_grid = 20
        # Calculate grid_start_y based on the modal's internal layout for title
        grid_start_y = (
            y + padding_around_grid + (UI_FONT_SIZE + 4) + padding_around_grid
        )

        num_rows = BAG_INVENTORY_ROWS
        num_cols = BAG_INVENTORY_COLS

        grid_width = (BAG_SLOT_SIZE * num_cols) + (BAG_SLOT_PADDING * (num_cols - 1))
        grid_start_x = x + (width - grid_width) // 2

        self.bag_slots = []  # Clear any existing slots
        for row in range(num_rows):
            for col in range(num_cols):
                slot_x = grid_start_x + col * (BAG_SLOT_SIZE + BAG_SLOT_PADDING)
                slot_y = grid_start_y + row * (BAG_SLOT_SIZE + BAG_SLOT_PADDING)
                bag_slot = BagCyberiaSlot(
                    slot_x,
                    slot_y,
                    BAG_SLOT_SIZE,
                    BAG_SLOT_SIZE,
                    object_layer_id_to_render=None,  # Content will be set later
                )
                self.bag_slots.append(bag_slot)
        self._initialized_grid = True

    def render_content(
        self,
        x: int,
        y: int,
        width: int,
        height: int,
        player_object_layer_ids: list[str],
        mouse_x: int,
        mouse_y: int,
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
            mouse_x: Current X coordinate of the mouse.
            mouse_y: Current Y coordinate of the mouse.
        """
        # Ensure grid is initialized if it's not already
        self._initialize_grid(x, y, width, height)

        padding_around_grid = 20
        current_y = y + padding_around_grid

        # Title for the bag view, dynamically set
        title_width = self.object_layer_render.measure_text(
            self.title_text, UI_FONT_SIZE + 4
        )  # Slightly larger font for title
        # set ' + (width - title_width) // 2' for center alignment
        title_x = x + 5
        self.object_layer_render.draw_text(
            self.title_text,
            title_x + 1,
            current_y + 1,
            UI_FONT_SIZE + 4,
            Color(*UI_TEXT_COLOR_SHADING),
        )
        self.object_layer_render.draw_text(
            self.title_text,
            title_x,
            current_y,
            UI_FONT_SIZE + 4,
            Color(*UI_TEXT_COLOR_PRIMARY),
        )
        current_y += (UI_FONT_SIZE + 4) + padding_around_grid  # Space after title

        # If an item is selected, stop rendering the grid and show item details instead
        if self.selected_object_layer_id:
            self._render_selected_item_details(
                x, y, width, height, self.selected_object_layer_id
            )
        else:
            self._render_inventory_grid(player_object_layer_ids, mouse_x, mouse_y)

    def _render_inventory_grid(
        self,
        player_object_layer_ids: list[str],
        mouse_x: int,
        mouse_y: int,
    ):
        """Renders the inventory grid using persistent slot objects."""
        skin_object_layer_ids = []
        for obj_layer_id in player_object_layer_ids:
            obj_def = self.object_layer_render.get_object_layer_definition(obj_layer_id)
            if obj_def and obj_def.get("TYPE") == "skin":
                skin_object_layer_ids.append(obj_layer_id)

        current_skin_index = 0
        for bag_slot in self.bag_slots:
            # Assign content to slots
            if current_skin_index < len(skin_object_layer_ids):
                bag_slot.object_layer_id_to_render = skin_object_layer_ids[
                    current_skin_index
                ]
                current_skin_index += 1
            else:
                bag_slot.object_layer_id_to_render = (
                    None  # Ensure empty slots are clear
                )

            # Render the slot, this also updates its hover state
            bag_slot.render(self.object_layer_render, mouse_x, mouse_y)

    def _render_selected_item_details(
        self, x: int, y: int, width: int, height: int, object_layer_id: str
    ):
        """Renders the details of the selected item."""
        # For now, just render the item in the center of the bag view
        # We can add more details (description, stats) later

        # Calculate center position for the item
        center_x = x + width / 2
        # center_y = y + height / 2
        center_y = y + 200

        # Define a larger size for the displayed item
        display_size = 150
        item_x = center_x - display_size / 2
        item_y = center_y - display_size / 2

        dummy_modal = ModalCoreComponent(
            screen_width=0,  # Not relevant for this specific usage
            screen_height=0,  # Not relevant for this specific usage
            render_content_callback=None,  # Will be overridden by the specific render function
            width=display_size,
            height=display_size,
        )
        dummy_modal.object_layer_id_to_render = object_layer_id

        render_modal_object_layer_item_content(
            dummy_modal,  # Pass the dummy component
            self.object_layer_render,
            int(item_x),
            int(item_y),
            int(display_size),
            int(display_size),
        )

    def handle_slot_clicks(
        self,
        modal_x: int,
        modal_y: int,
        modal_width: int,
        modal_height: int,
        player_object_layer_ids: list[str],  # Needed to determine if a slot is occupied
        mouse_x: int,
        mouse_y: int,
        is_mouse_button_pressed: bool,
    ) -> bool:
        """
        Processes clicks on the inventory slots. Returns True if any slot within the bag
        modal was clicked (regardless of content), preventing world clicks.
        """
        if self.selected_object_layer_id:
            return False  # No grid clicks when an item is selected

        # Ensure grid is initialized before checking clicks on its slots
        self._initialize_grid(modal_x, modal_y, modal_width, modal_height)

        clicked_a_slot = False

        # Reset all slots' clicked state before checking for a new click
        for bag_slot in self.bag_slots:
            bag_slot.is_clicked = False

        for bag_slot in self.bag_slots:
            # Check if this specific slot was clicked
            if bag_slot.check_click(mouse_x, mouse_y, is_mouse_button_pressed):
                clicked_a_slot = True
                # If an occupied slot was clicked, set the selected item and update title
                if bag_slot.object_layer_id_to_render:
                    self.selected_object_layer_id = bag_slot.object_layer_id_to_render
                    self.title_text = (
                        self.selected_object_layer_id
                    )  # Update title to clicked item's name
                    bag_slot.is_clicked = True  # Keep this slot in clicked state
                    logging.info(f"Bag slot clicked: {self.selected_object_layer_id}")
                else:
                    logging.debug("Empty bag slot clicked.")
                break  # A slot was clicked, consume the event

        return clicked_a_slot

    def reset_view(self):
        """Resets the bag view to its default state (grid view, default title)."""
        self.selected_object_layer_id = None
        self.title_text = "Bag"
        # Reset the object_layer_id_to_render for all slots when resetting the view
        for slot in self.bag_slots:
            slot.object_layer_id_to_render = None
            slot.is_clicked = False  # Also reset clicked state
        logging.info("Bag view reset to default grid view.")
