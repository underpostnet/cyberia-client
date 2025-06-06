import logging
from raylibpy import Color, Vector2, RAYWHITE
from config import (
    UI_FONT_SIZE,
    UI_TEXT_COLOR_PRIMARY,
    UI_TEXT_COLOR_SHADING,
)
from object_layer.object_layer_render import ObjectLayerRender
from ui.components.core.grid_core_component import GridCoreComponent

from object_layer.object_layer_data import (
    Direction,
    ObjectLayerMode,
)

logging.basicConfig(
    level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s"
)

# Explicitly set bag inventory constants
BAG_INVENTORY_ROWS = 5
BAG_INVENTORY_COLS = 6
BAG_SLOT_SIZE = 40
BAG_SLOT_PADDING = 10


class BagCyberiaView:
    """
    Manages the display and interaction for the player's inventory bag interface.
    It can display a grid of items or a detailed view of a single item.
    """

    def __init__(self, object_layer_render_instance: ObjectLayerRender):
        self.object_layer_render = object_layer_render_instance
        self.title_text = "Bag"
        self.player_object_layer_ids = []  # Stores the player's "skins"

        # Track selected item to switch between grid view and single item detail view
        self.selected_object_layer_id: str | None = None
        self.selected_slot_index: int | None = None

        # Initialize GridCoreComponent for the inventory grid
        self.grid_component = GridCoreComponent(
            object_layer_render_instance=self.object_layer_render,
            num_rows=BAG_INVENTORY_ROWS,
            num_cols=BAG_INVENTORY_COLS,
            item_width=BAG_SLOT_SIZE,
            item_height=BAG_SLOT_SIZE,
            item_padding=BAG_SLOT_PADDING,
            render_item_callback=self._render_bag_slot_item,
            background_color=Color(
                0, 0, 0, 0
            ),  # Transparent, parent modal draws background
            slot_background_color=Color(30, 30, 30, 180),
            slot_hover_color=Color(50, 50, 50, 200),
            slot_selected_color=Color(100, 100, 0, 200),
        )

    def _render_bag_slot_item(
        self,
        object_layer_render_instance: ObjectLayerRender,
        x: int,
        y: int,
        width: int,
        height: int,
        item_object_layer_id: str,  # item_data is the object_layer_id in this context
        is_hovered: bool,
        is_selected: bool,
    ):
        """
        Renders a single inventory item within a grid slot.
        This is a callback passed to GridCoreComponent.
        """
        # Get the object layer definition to determine matrix dimension and other properties
        object_layer_info = object_layer_render_instance.get_object_layer_definition(
            item_object_layer_id
        )
        if object_layer_info:
            object_layer_info_render_data = object_layer_info.get("RENDER_DATA")
            if not object_layer_info_render_data:
                logging.warning(
                    f"No RENDER_DATA found for object layer ID: {item_object_layer_id}. Cannot render item."
                )
                return
        else:
            logging.warning(
                f"No object layer definition found for ID: {item_object_layer_id}. Cannot render item."
            )
            return

        matrix_dimension = (
            object_layer_render_instance.get_object_layer_matrix_dimension(
                item_object_layer_id
            )
        )
        if matrix_dimension == 0:
            logging.warning(
                f"Matrix dimension is 0 for {item_object_layer_id}. Cannot render item."
            )
            return

        # Calculate pixel size to fit exactly within the slot
        pixel_size_in_display = width // matrix_dimension
        if pixel_size_in_display == 0:
            pixel_size_in_display = 1

        # Calculate the total rendered size of the animation
        rendered_width = pixel_size_in_display * matrix_dimension
        rendered_height = pixel_size_in_display * matrix_dimension

        # Calculate offset to center the rendered animation within the slot
        offset_x_centering = (width - rendered_width) // 2
        offset_y_centering = (height - rendered_height) // 2

        # Get or create the animation instance for this specific item in the bag context
        anim_props = object_layer_render_instance.get_or_create_object_layer_animation(
            obj_id=f"bag_item_{item_object_layer_id}",
            object_layer_id=item_object_layer_id,
            target_object_layer_size_pixels=pixel_size_in_display,
            initial_direction=Direction.DOWN,  # Default direction for inventory item display
        )
        anim_instance = anim_props["object_layer_animation_instance"]

        # Set animation to a static idle state or a gentle animation for inventory
        anim_instance.set_state(
            Direction.DOWN, ObjectLayerMode.IDLE, 0.0
        )  # Using IDLE for static item representation

        # Get the frame matrix and color map from the current animation state
        frame_matrix, color_map, _, _ = anim_instance.get_current_frame_data(
            object_layer_render_instance.get_frame_time()
        )

        self.object_layer_render.render_specific_object_layer_frame(
            frame_matrix=frame_matrix,
            color_map=color_map,
            screen_x=x + offset_x_centering,
            screen_y=y + offset_y_centering,
            pixel_size_in_display=pixel_size_in_display,
        )

        # Display item name (object_layer_id) below the item
        item_name = item_object_layer_id.replace("_", " ").title()  # Basic formatting
        text_width = object_layer_render_instance.measure_text(
            item_name, UI_FONT_SIZE - 4
        )
        text_x = x + (width - text_width) // 2
        text_y = y + height - (UI_FONT_SIZE - 4) - 5  # 5 pixels from bottom

        self.object_layer_render.draw_text(
            item_name,
            text_x + 1,
            text_y + 1,
            UI_FONT_SIZE - 4,
            Color(*UI_TEXT_COLOR_SHADING),
        )
        self.object_layer_render.draw_text(
            item_name,
            text_x,
            text_y,
            UI_FONT_SIZE - 4,
            Color(*UI_TEXT_COLOR_PRIMARY),
        )

    def _render_single_item_detail(
        self,
        modal_component,  # Added modal_component to access its set_title method
        x: int,
        y: int,
        width: int,
        height: int,
        player_object_layer_ids: list[str],
        mouse_x: int,  # Added for button hover state
        mouse_y: int,  # Added for button hover state
        is_mouse_button_pressed: bool,  # Added for button click
    ):
        """
        Renders the detailed view of a single selected item.
        """
        if not self.selected_object_layer_id:
            return

        # Title for the item
        item_name = self.selected_object_layer_id.replace("_", " ").title()
        self.title_text = item_name  # Update internal title
        modal_component.set_title(self.title_text)  # Set the modal's title

        text_width = self.object_layer_render.measure_text(item_name, UI_FONT_SIZE + 5)
        self.object_layer_render.draw_text(
            item_name,
            x + (width - text_width) // 2 + 1,
            y + 20 + 1,
            UI_FONT_SIZE + 5,
            Color(*UI_TEXT_COLOR_SHADING),
        )
        self.object_layer_render.draw_text(
            item_name,
            x + (width - text_width) // 2,
            y + 20,
            UI_FONT_SIZE + 5,
            Color(*UI_TEXT_COLOR_PRIMARY),
        )

        # Render the large item animation in the center
        item_display_size = min(width, height) * 0.5  # 50% of modal's smaller dimension
        matrix_dimension = self.object_layer_render.get_object_layer_matrix_dimension(
            self.selected_object_layer_id
        )
        pixel_size = item_display_size / matrix_dimension if matrix_dimension > 0 else 1

        centered_x = x + (width - item_display_size) / 2
        centered_y = (
            y + (height - item_display_size) / 2 - 50
        )  # Adjust up to make space for text

        # Get or create the animation instance for this specific item in the bag context
        anim_props = self.object_layer_render.get_or_create_object_layer_animation(
            obj_id=f"bag_item_detail_{self.selected_object_layer_id}",
            object_layer_id=self.selected_object_layer_id,
            target_object_layer_size_pixels=pixel_size,
            initial_direction=Direction.DOWN,  # Default direction for inventory item display
        )
        anim_instance = anim_props["object_layer_animation_instance"]
        anim_instance.set_state(
            Direction.DOWN, ObjectLayerMode.WALKING, 0.0
        )  # Animate in detail view

        frame_matrix, color_map, _, _ = anim_instance.get_current_frame_data(
            self.object_layer_render.get_frame_time()
        )

        self.object_layer_render.render_specific_object_layer_frame(
            frame_matrix=frame_matrix,
            color_map=color_map,
            screen_x=centered_x,
            screen_y=centered_y,
            pixel_size_in_display=pixel_size,
        )

        # Description text
        description = f"This is a {item_name} object layer for your character."
        self.object_layer_render.draw_text(
            description,
            x + 20 + 1,
            centered_y + item_display_size + 20 + 1,
            UI_FONT_SIZE - 2,
            Color(*UI_TEXT_COLOR_SHADING),
        )
        self.object_layer_render.draw_text(
            description,
            x + 20,
            centered_y + item_display_size + 20,
            UI_FONT_SIZE - 2,
            Color(*UI_TEXT_COLOR_PRIMARY),
        )

        # "Equip" button (example functionality)
        button_width = 100
        button_height = 30
        button_x = x + (width - button_width) // 2
        button_y = centered_y + item_display_size + 80

        button_color = Color(0, 121, 241, 200)  # Blueish
        button_text = "Equip"

        # Check if item is already equipped
        is_equipped = self.selected_object_layer_id in player_object_layer_ids
        if is_equipped:
            button_text = "Equipped"
            button_color = Color(50, 150, 50, 200)  # Greenish for equipped

        # Handle button hover and click
        is_button_hovered = (
            mouse_x >= button_x
            and mouse_x <= (button_x + button_width)
            and mouse_y >= button_y
            and mouse_y <= (button_y + button_height)
        )

        if is_button_hovered and not is_equipped:
            button_color.a = 255  # Make more opaque on hover

        self.object_layer_render.draw_rectangle(
            button_x, button_y, button_width, button_height, button_color
        )
        btn_text_width = self.object_layer_render.measure_text(
            button_text, UI_FONT_SIZE - 2
        )
        self.object_layer_render.draw_text(
            button_text,
            button_x + (button_width - btn_text_width) // 2 + 1,
            button_y + (button_height - (UI_FONT_SIZE - 2)) // 2 + 1,
            UI_FONT_SIZE - 2,
            Color(*UI_TEXT_COLOR_SHADING),
        )
        self.object_layer_render.draw_text(
            button_text,
            button_x + (button_width - btn_text_width) // 2,
            button_y + (button_height - (UI_FONT_SIZE - 2)) // 2,
            UI_FONT_SIZE - 2,
            Color(*UI_TEXT_COLOR_PRIMARY),
        )

    def render_content(
        self,
        modal_component,  # Added modal_component
        x: int,
        y: int,
        width: int,
        height: int,
        player_object_layer_ids: list[str],
        mouse_x: int,
        mouse_y: int,
        is_mouse_button_pressed: bool,
    ):
        """
        Main render method for the bag view.
        Switches between grid view and single item detail view based on self.selected_object_layer_id.
        """
        # Filter out "ACTION_AREA" but keep "ANON" as it can be a skin
        equippable_items = [
            obj_id
            for obj_id in player_object_layer_ids
            if obj_id not in ["ACTION_AREA"]
        ]

        if self.selected_object_layer_id is None:
            # Render the inventory grid
            self.title_text = "Bag"  # Reset internal title for grid view
            modal_component.set_title(self.title_text)  # Set the modal's title

            # Draw the title for the grid
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

            # Adjust grid offset to make space for the title
            grid_offset_y = y + title_font_size + 30  # 30 pixels padding after title

            # Calculate horizontal offset to center the grid using the new method
            centered_grid_x_offset = self.grid_component.calculate_centered_offset_x(
                width
            )

            self.grid_component.render(
                offset_x=x + centered_grid_x_offset,  # Apply centering offset
                offset_y=grid_offset_y,
                container_width=width,
                container_height=height
                - (
                    grid_offset_y - y
                ),  # Reduce container height by the space taken by title
                items_data=equippable_items,  # Pass only equippable items to the grid
                mouse_x=mouse_x,
                mouse_y=mouse_y,
                selected_index=self.selected_slot_index,  # Keep track of selected slot
            )
        else:
            # Render single item detail view
            self._render_single_item_detail(
                modal_component,  # Pass modal_component
                x,
                y,
                width,
                height,
                player_object_layer_ids,
                mouse_x,
                mouse_y,
                is_mouse_button_pressed,
            )

    def handle_slot_clicks(
        self,
        offset_x: int,
        offset_y: int,
        container_width: int,
        container_height: int,
        player_object_layer_ids: list[str],
        mouse_x: int,
        mouse_y: int,
        is_mouse_button_pressed: bool,
    ) -> bool:
        """
        Handles clicks within the bag modal. Returns True if a click was handled.
        """
        if (
            not is_mouse_button_pressed
        ):  # Only process clicks when the mouse button is initially pressed
            return False

        if self.selected_object_layer_id is None:
            # In grid view, check for slot clicks
            # Filter out "ACTION_AREA" but keep "ANON" as it can be a skin
            equippable_items = [
                obj_id
                for obj_id in player_object_layer_ids
                if obj_id not in ["ACTION_AREA"]
            ]

            # Adjust offset for grid click detection, similar to rendering
            title_font_size = UI_FONT_SIZE + 2
            grid_offset_y = offset_y + title_font_size + 30

            # Calculate horizontal offset to center the grid for click detection
            centered_grid_x_offset = self.grid_component.calculate_centered_offset_x(
                container_width
            )

            clicked_index = self.grid_component.get_clicked_item_index(
                offset_x=offset_x + centered_grid_x_offset,  # Apply centering offset
                offset_y=grid_offset_y,  # Use adjusted offset for click detection
                mouse_x=mouse_x,
                mouse_y=mouse_y,
                is_mouse_button_pressed=is_mouse_button_pressed,  # Pass the actual state
            )
            if clicked_index is not None and clicked_index < len(equippable_items):
                self.selected_object_layer_id = equippable_items[clicked_index]
                self.selected_slot_index = clicked_index
                logging.info(f"Selected item: {self.selected_object_layer_id}")
                return True
            return False
        else:
            # In single item detail view, check for button clicks (e.g., Equip button)
            button_width = 100
            button_height = 30
            item_display_size = min(container_width, container_height) * 0.5
            button_x = offset_x + (container_width - button_width) // 2
            button_y = (
                (offset_y + (container_height - item_display_size) / 2 - 50)
                + item_display_size
                + 80
            )  # Recalculate button_y

            is_equipped = self.selected_object_layer_id in player_object_layer_ids
            if (
                is_mouse_button_pressed
                and mouse_x >= button_x
                and mouse_x <= (button_x + button_width)
                and mouse_y >= button_y
                and mouse_y <= (button_y + button_height)
                and not is_equipped  # Only allow click if not already equipped
            ):
                logging.info(
                    f"Equip button clicked for {self.selected_object_layer_id}"
                )
                # Here you would typically send a message to the server to equip the item
                # For now, we'll just log and potentially update the player's object_layer_ids in a real game
                # This logic is illustrative, actual equipping would modify the player's object
                return True  # Indicate click handled
            return False

    def reset_view(self):
        """Resets the bag view to its initial grid state."""
        self.selected_object_layer_id = None
        self.selected_slot_index = None
        self.title_text = "Bag"
