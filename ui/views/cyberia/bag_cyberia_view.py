import logging
from raylibpy import Color, Vector2, RAYWHITE
from config import UI_FONT_SIZE, UI_TEXT_COLOR_PRIMARY, UI_TEXT_COLOR_SHADING
from ui.components.core.grid_core_component import GridCoreComponent
from object_layer.object_layer_render import ObjectLayerRender
from object_layer.object_layer_data import (
    Direction,
    ObjectLayerMode,
)  # Needed for rendering items

logging.basicConfig(
    level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s"
)

# Removed EQUIPPABLE_ITEM_IDS as items will now be dynamically drawn based on player's object_layer_ids


class BagCyberiaView:
    """
    Manages the display and interaction for the player's inventory (bag) interface.
    Displays items in a grid and allows selection for detail view.
    """

    def __init__(self, object_layer_render_instance: ObjectLayerRender):
        self.object_layer_render = object_layer_render_instance
        self.title_text = "Bag"
        # This will hold the actual items in the player's bag, received dynamically
        self.bag_items: list[str] = []  # List of object_layer_ids in the bag
        self.selected_object_layer_id: str | None = (
            None  # Track selected item for detail view
        )

        # Configure GridCoreComponent for the bag grid
        self.grid_component = GridCoreComponent(
            object_layer_render_instance=self.object_layer_render,
            num_rows=5,  # Example: 5 rows
            num_cols=4,  # Example: 4 columns
            item_width=50,  # Size of each item slot
            item_height=50,
            item_padding=10,  # Padding between items
            render_item_callback=self._render_bag_item,
            background_color=Color(0, 0, 0, 0),  # Transparent, main modal draws it
            border_color=Color(50, 50, 50, 200),
            slot_background_color=Color(30, 30, 30, 180),
            slot_hover_color=Color(50, 50, 50, 200),
            slot_selected_color=Color(150, 150, 0, 200),
            grid_type="rectangle",
        )

    def _render_bag_item(
        self,
        object_layer_render_instance: ObjectLayerRender,
        x: int,
        y: int,
        width: int,
        height: int,
        item_id: str,  # This will be the object_layer_id string
        is_hovered: bool,
        is_selected: bool,
    ):
        """
        Renders the content of a single bag item slot.
        Displays the object layer animation or a placeholder within the slot.
        """
        if item_id is None:  # For empty slots if the bag isn't full
            placeholder_text = "Empty"
            text_font_size = UI_FONT_SIZE - 4
            text_width = object_layer_render_instance.measure_text(
                placeholder_text, text_font_size
            )
            text_x = x + (width - text_width) // 2
            text_y = y + (height - text_font_size) // 2
            object_layer_render_instance.draw_text(
                placeholder_text,
                text_x + 1,
                text_y + 1,
                text_font_size,
                Color(*UI_TEXT_COLOR_SHADING),
            )
            object_layer_render_instance.draw_text(
                placeholder_text,
                text_x,
                text_y,
                text_font_size,
                Color(*UI_TEXT_COLOR_PRIMARY),
            )
            return

        # Get object layer definition to render the item
        object_layer_data = object_layer_render_instance.get_object_layer_definition(
            item_id
        )
        if not object_layer_data:
            logging.warning(
                f"No definition found for object layer ID: {item_id}. Cannot render bag item."
            )
            placeholder_text = "Error"
            text_font_size = UI_FONT_SIZE - 4
            text_width = object_layer_render_instance.measure_text(
                placeholder_text, text_font_size
            )
            text_x = x + (width - text_width) // 2
            text_y = y + (height - text_font_size) // 2
            object_layer_render_instance.draw_text(
                placeholder_text,
                text_x + 1,
                text_y + 1,
                text_font_size,
                Color(*UI_TEXT_COLOR_SHADING),
            )
            object_layer_render_instance.draw_text(
                placeholder_text,
                text_x,
                text_y,
                text_font_size,
                Color(*UI_TEXT_COLOR_PRIMARY),
            )
            return

        matrix_dimension = (
            object_layer_render_instance.get_object_layer_matrix_dimension(item_id)
        )
        if matrix_dimension == 0:
            logging.warning(
                f"Matrix dimension is 0 for {item_id}. Cannot render item. Trying to set to 1."
            )
            matrix_dimension = (
                1  # Prevent division by zero, allow minimal rendering if possible
            )

        pixel_size_in_display = width // matrix_dimension
        if pixel_size_in_display == 0:
            pixel_size_in_display = 1  # Ensure pixel size is at least 1

        rendered_width = pixel_size_in_display * matrix_dimension
        rendered_height = pixel_size_in_display * matrix_dimension

        # Center the item within its slot
        offset_x_centering = (width - rendered_width) // 2
        offset_y_centering = (height - rendered_height) // 2

        # Determine animation mode based on item type and statelessness
        anim_mode = ObjectLayerMode.IDLE
        if object_layer_data.get("TYPE") == "skin" and not object_layer_data.get(
            "IS_STATELESS"
        ):
            anim_mode = ObjectLayerMode.WALKING

        # Get or create animation instance for the item
        # Use a unique obj_id for bag items, e.g., "bag_item_{item_id}"
        anim_props = object_layer_render_instance.get_or_create_object_layer_animation(
            obj_id=f"bag_item_{item_id}",
            object_layer_id=item_id,
            target_object_layer_size_pixels=pixel_size_in_display,
            initial_direction=Direction.DOWN,  # Default direction for static display
        )
        anim_instance = anim_props["object_layer_animation_instance"]
        # Set state based on determined animation mode
        anim_instance.set_state(Direction.DOWN, anim_mode, 0.0)

        # Get current frame data
        frame_matrix, color_map, _, _ = anim_instance.get_current_frame_data(
            object_layer_render_instance.get_frame_time()
        )

        # Render the frame
        object_layer_render_instance.render_specific_object_layer_frame(
            frame_matrix=frame_matrix,
            color_map=color_map,
            screen_x=x + offset_x_centering,
            screen_y=y + offset_y_centering,
            pixel_size_in_display=pixel_size_in_display,
        )

    def _render_single_item_detail(
        self,
        modal_component,  # Added modal_component to access its set_title method
        x: int,
        y: int,
        width: int,
        height: int,
        mouse_x: int,
        mouse_y: int,
        is_mouse_button_pressed: bool,
    ):
        """
        Renders the detailed view of a single selected item from the bag.
        """
        if not self.selected_object_layer_id:
            modal_component.set_title("No Item Selected")
            empty_text = "Select an item from your bag for details."
            text_width = self.object_layer_render.measure_text(empty_text, UI_FONT_SIZE)
            self.object_layer_render.draw_text(
                empty_text,
                x + (width - text_width) // 2 + 1,
                y + height // 2 + 1,
                UI_FONT_SIZE,
                Color(*UI_TEXT_COLOR_SHADING),
            )
            self.object_layer_render.draw_text(
                empty_text,
                x + (width - text_width) // 2,
                y + height // 2,
                UI_FONT_SIZE,
                Color(*UI_TEXT_COLOR_PRIMARY),
            )
            return

        item_id = self.selected_object_layer_id
        item_name = item_id.replace("_", " ").title()  # Convert ID to a readable name
        self.title_text = item_name  # Update BagCyberiaView's internal title
        modal_component.set_title(self.title_text)  # Set the modal's title

        text_color = Color(*UI_TEXT_COLOR_PRIMARY)
        shading_color = Color(*UI_TEXT_COLOR_SHADING)

        # Draw Item Name
        title_width = self.object_layer_render.measure_text(item_name, UI_FONT_SIZE + 5)
        self.object_layer_render.draw_text(
            item_name,
            x + (width - title_width) // 2 + 1,
            y + 20 + 1,
            UI_FONT_SIZE + 5,
            shading_color,
        )
        self.object_layer_render.draw_text(
            item_name,
            x + (width - title_width) // 2,
            y + 20,
            UI_FONT_SIZE + 5,
            text_color,
        )

        # Render the item's animation large in the detail view
        item_display_size = (
            min(width, height) * 0.4
        )  # Make it occupy 40% of the modal's smaller dimension
        matrix_dimension = self.object_layer_render.get_object_layer_matrix_dimension(
            item_id
        )
        pixel_size = item_display_size / matrix_dimension if matrix_dimension > 0 else 1

        centered_x = x + (width - item_display_size) / 2
        centered_y = y + 80  # Position below title

        # Determine animation mode based on item type and statelessness for detail view
        object_layer_data = self.object_layer_render.get_object_layer_definition(
            item_id
        )
        anim_mode = ObjectLayerMode.IDLE
        if (
            object_layer_data
            and object_layer_data.get("TYPE") == "skin"
            and not object_layer_data.get("IS_STATELESS")
        ):
            anim_mode = ObjectLayerMode.WALKING

        # Get or create animation instance for the item in detail view
        anim_props = self.object_layer_render.get_or_create_object_layer_animation(
            obj_id=f"bag_detail_item_{item_id}",
            object_layer_id=item_id,
            target_object_layer_size_pixels=pixel_size,
            initial_direction=Direction.DOWN,  # Default direction for static display
        )
        anim_instance = anim_props["object_layer_animation_instance"]
        anim_instance.set_state(Direction.DOWN, anim_mode, 0.0)

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

        # Item Description/Details
        description_text = f"This is an example description for the {item_name} item. It can be equipped or used for crafting."
        desc_start_y = centered_y + item_display_size + 20
        max_desc_width = width - 40  # 20px padding on each side
        lines = self._wrap_text(description_text, max_desc_width, UI_FONT_SIZE - 2)

        current_y = desc_start_y
        for line in lines:
            self.object_layer_render.draw_text(
                line, x + 20 + 1, current_y + 1, UI_FONT_SIZE - 2, shading_color
            )
            self.object_layer_render.draw_text(
                line, x + 20, current_y, UI_FONT_SIZE - 2, text_color
            )
            current_y += (UI_FONT_SIZE - 2) + 5  # Line height + spacing

        # "Equip" or "Use" button
        button_width = 120
        button_height = 30
        button_x = x + (width - button_width) // 2
        button_y = height - button_height - 20  # Position near bottom

        button_color = Color(0, 121, 241, 200)  # Blueish
        button_text = "Equip"  # Placeholder, actual logic needed

        # Handle button hover
        is_button_hovered = (
            mouse_x >= button_x
            and mouse_x <= (button_x + button_width)
            and mouse_y >= button_y
            and mouse_y <= (button_y + button_height)
        )
        if is_button_hovered:
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
            shading_color,
        )
        self.object_layer_render.draw_text(
            button_text,
            button_x + (button_width - btn_text_width) // 2,
            button_y + (button_height - (UI_FONT_SIZE - 2)) // 2,
            UI_FONT_SIZE - 2,
            text_color,
        )

    def _wrap_text(self, text: str, max_width: int, font_size: int) -> list[str]:
        """Wraps text to fit within a maximum width."""
        words = text.split(" ")
        lines = []
        current_line = []
        for word in words:
            test_line = " ".join(current_line + [word])
            if self.object_layer_render.measure_text(test_line, font_size) <= max_width:
                current_line.append(word)
            else:
                lines.append(" ".join(current_line))
                current_line = [word]
        if current_line:
            lines.append(" ".join(current_line))
        return lines

    def render_content(
        self,
        modal_component,  # Added modal_component
        x: int,
        y: int,
        width: int,
        height: int,
        player_object_layer_ids: list[str],  # Now explicitly passed
        mouse_x: int,
        mouse_y: int,
        is_mouse_button_pressed: bool,
    ):
        """
        Main render method for the bag view.
        Switches between grid view and single item detail view.
        """
        # Update bag_items based on player's current object_layer_ids that have animation definitions
        self.bag_items = [
            item_id
            for item_id in player_object_layer_ids
            if self.object_layer_render.get_object_layer_definition(item_id) is not None
        ]

        if self.selected_object_layer_id is None:
            # Render the bag grid
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

            # Calculate horizontal offset to center the grid
            centered_grid_x_offset = self.grid_component.calculate_centered_offset_x(
                width
            )

            # Fill empty slots in the grid with None for proper rendering of empty cells
            display_items = self.bag_items + [None] * (
                self.grid_component.num_rows * self.grid_component.num_cols
                - len(self.bag_items)
            )

            self.grid_component.render(
                offset_x=x + centered_grid_x_offset,
                offset_y=grid_offset_y,
                container_width=width,
                container_height=height - (grid_offset_y - y),
                items_data=display_items,  # Pass item IDs directly
                mouse_x=mouse_x,
                mouse_y=mouse_y,
                selected_index=(
                    self.bag_items.index(self.selected_object_layer_id)
                    if self.selected_object_layer_id in self.bag_items
                    else None
                ),
            )
        else:
            # Render single item detail view
            self._render_single_item_detail(
                modal_component,  # Pass modal_component
                x,
                y,
                width,
                height,
                mouse_x,
                mouse_y,
                is_mouse_button_pressed,
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
        Handles clicks within the bag modal. Returns True if a click was handled.
        """
        if not is_mouse_button_pressed:
            return False

        if self.selected_object_layer_id is None:
            # In grid view, check for item clicks
            title_font_size = UI_FONT_SIZE + 2
            grid_offset_y = offset_y + title_font_size + 30
            centered_grid_x_offset = self.grid_component.calculate_centered_offset_x(
                container_width
            )

            clicked_index = self.grid_component.get_clicked_item_index(
                offset_x=offset_x + centered_grid_x_offset,
                offset_y=grid_offset_y,
                mouse_x=mouse_x,
                mouse_y=mouse_y,
                is_mouse_button_pressed=is_mouse_button_pressed,
            )

            # Check if a valid item was clicked (not an empty slot)
            if (
                clicked_index is not None
                and clicked_index < len(self.bag_items)
                and self.bag_items[clicked_index] is not None
            ):
                self.selected_object_layer_id = self.bag_items[clicked_index]
                logging.info(f"Bag item clicked: {self.selected_object_layer_id}")
                return True
            return False
        else:
            # In single item detail view, check for "Equip/Use" button click
            button_width = 120
            button_height = 30
            button_x = offset_x + (container_width - button_width) // 2
            button_y = container_height - button_height - 20  # From bottom of modal

            if (
                mouse_x >= button_x
                and mouse_x <= (button_x + button_width)
                and mouse_y >= button_y
                and mouse_y <= (button_y + button_height)
            ):
                logging.info(
                    f"Equip/Use button clicked for {self.selected_object_layer_id}"
                )
                # Placeholder for equip/use logic (e.g., send to server, update character view)
                # For now, let's just log and then reset view
                # In a real game, this would involve sending a message to the server.
                self.reset_view()  # Go back to grid view after action
                return True
            return False

    def reset_view(self):
        """Resets the view state, e.g., deselects any selected item."""
        self.selected_object_layer_id = None
        self.title_text = "Bag"
