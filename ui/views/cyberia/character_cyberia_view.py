import logging
import math
from raylibpy import Color, Vector2, RAYWHITE
from config import (
    UI_FONT_SIZE,
    UI_TEXT_COLOR_PRIMARY,
    UI_TEXT_COLOR_SHADING,
)
from object_layer.object_layer_render import ObjectLayerRender
from ui.components.core.grid_core_component import (
    GridCoreComponent,
)  # Import GridCoreComponent
from object_layer.object_layer_data import (
    Direction,
    ObjectLayerMode,
)  # Import Direction and ObjectLayerMode

logging.basicConfig(
    level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s"
)

# Constants for the character view layout
# CHARACTER_SLOT_WIDTH/HEIGHT now refer to the inner content area within the hexagon
CHARACTER_SLOT_WIDTH = 60
CHARACTER_SLOT_HEIGHT = 60
CHARACTER_HEXAGON_VISUAL_RADIUS = 40  # This controls the size of the drawn hexagon


# Placeholder for empty slot in character view
EMPTY_SLOT_ID = "EMPTY_SLOT"  # Using a placeholder for now


class CharacterCyberiaView:
    """
    Manages the display and interaction for the player's character equipment interface.
    Displays 7 slots in a hexagonal arrangement, with visually hexagonal slots.
    Uses GridCoreComponent for rendering and click detection.
    """

    def __init__(self, object_layer_render_instance: ObjectLayerRender):
        self.object_layer_render = object_layer_render_instance
        self.title_text = "Character"

        # These would typically come from player data/server
        self.equipped_object_layer_ids = {
            "center": "ANON",  # Player's current skin
            "head": EMPTY_SLOT_ID,
            "chest": EMPTY_SLOT_ID,
            "hands": EMPTY_SLOT_ID,
            "legs": EMPTY_SLOT_ID,
            "feet": EMPTY_SLOT_ID,
            "weapon": EMPTY_SLOT_ID,
        }
        # The slot order is conceptually for the hexagonal layout, but handled by grid_layout
        self.slot_order = [
            "head",
            "weapon",
            "chest",
            "hands",
            "legs",
            "feet",
        ]

        # Map for grid-based rendering: row, col, and the actual slot key
        self.grid_slots_map = self._create_character_grid_map()

        # Track selected item for detail view
        self.selected_slot_key: str | None = None
        self.selected_object_layer_id: str | None = None

        # Initialize GridCoreComponent for the 3x3 conceptual grid that holds the 7 slots
        self.grid_component = GridCoreComponent(
            object_layer_render_instance=self.object_layer_render,
            num_rows=3,  # Fixed to 3 rows for the 3x3 conceptual grid
            num_cols=3,  # Fixed to 3 columns for the 3x3 conceptual grid
            item_width=CHARACTER_SLOT_WIDTH,  # Inner content size
            item_height=CHARACTER_SLOT_HEIGHT,  # Inner content size
            item_padding=0,  # Padding is implicitly handled by hexagon positioning and radius
            render_item_callback=self._render_character_slot_item,
            background_color=Color(0, 0, 0, 0),  # Transparent, main modal draws it
            border_color=Color(50, 50, 50, 200),
            slot_background_color=Color(30, 30, 30, 180),
            slot_hover_color=Color(50, 50, 50, 200),
            slot_selected_color=Color(100, 100, 0, 200),
            grid_type="hexagon",  # Specify hexagon grid type
            hexagon_radius=CHARACTER_HEXAGON_VISUAL_RADIUS,  # Pass the visual radius for hexagons
        )

    def _create_character_grid_map(self) -> list[dict]:
        """
        Creates a list representing the conceptual 3x3 rectangular grid for character equipment,
        mapping grid indices to actual slot keys.
        The layout is designed to visually represent a hexagon with a center slot.
        """
        # Define the 3x3 grid layout with slot keys or None for empty visual cells
        # This determines the visual arrangement of the square slots
        grid_layout_keys = [
            "head",
            None,
            "weapon",
            "hands",
            "center",
            "chest",
            "feet",
            None,
            "legs",
        ]

        processed_grid_layout = []
        for slot_key in grid_layout_keys:
            if slot_key:
                processed_grid_layout.append(
                    {
                        "key": slot_key,
                        "item_id": self.equipped_object_layer_ids.get(
                            slot_key, EMPTY_SLOT_ID
                        ),
                    }
                )
            else:
                # For None slots, include a placeholder dictionary
                processed_grid_layout.append({"key": None, "item_id": EMPTY_SLOT_ID})
        return processed_grid_layout

    def _render_character_slot_item(
        self,
        object_layer_render_instance: ObjectLayerRender,
        x: int,
        y: int,
        width: int,
        height: int,
        item_data: dict,  # Receives a dict containing 'key' and 'item_id'
        is_hovered: bool,
        is_selected: bool,
    ):
        """
        Renders a single character slot item.
        Displays the object layer animation or a placeholder within the slot.
        """
        slot_key = item_data.get("key")
        item_object_layer_id = item_data.get("item_id")

        # Draw slot name *above* the item, centered within the slot's width
        if slot_key:  # Only draw slot name if it's an actual slot
            slot_name_text = slot_key.replace("_", " ").title()
            text_font_size = UI_FONT_SIZE - 6  # Smaller font for slot names
            text_width = object_layer_render_instance.measure_text(
                slot_name_text, text_font_size
            )
            text_x = x + (width - text_width) // 2
            text_y = y - text_font_size - 2  # Position above the slot

            object_layer_render_instance.draw_text(
                slot_name_text,
                text_x + 1,
                text_y + 1,
                text_font_size,
                Color(*UI_TEXT_COLOR_SHADING),
            )
            object_layer_render_instance.draw_text(
                slot_name_text,
                text_x,
                text_y,
                text_font_size,
                Color(*UI_TEXT_COLOR_PRIMARY),
            )

        # Ensure item_object_layer_id is valid for rendering, otherwise use placeholder
        if (
            item_object_layer_id == EMPTY_SLOT_ID
            or not object_layer_render_instance.get_object_layer_definition(
                item_object_layer_id
            )
        ):
            # Draw an 'Empty' placeholder if no item or item definition is missing
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

        # Render the actual item's animation
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

        pixel_size_in_display = width // matrix_dimension
        if pixel_size_in_display == 0:
            pixel_size_in_display = 1  # Ensure pixel size is at least 1

        rendered_width = pixel_size_in_display * matrix_dimension
        rendered_height = pixel_size_in_display * matrix_dimension

        # Center the item within its slot's content area
        offset_x_centering = (width - rendered_width) // 2
        offset_y_centering = (height - rendered_height) // 2

        anim_props = object_layer_render_instance.get_or_create_object_layer_animation(
            obj_id=f"char_slot_item_{item_object_layer_id}",
            object_layer_id=item_object_layer_id,
            target_object_layer_size_pixels=pixel_size_in_display,
            initial_direction=Direction.DOWN,
        )
        anim_instance = anim_props["object_layer_animation_instance"]
        anim_instance.set_state(Direction.DOWN, ObjectLayerMode.IDLE, 0.0)

        frame_matrix, color_map, _, _ = anim_instance.get_current_frame_data(
            object_layer_render_instance.get_frame_time()
        )

        object_layer_render_instance.render_specific_object_layer_frame(
            frame_matrix=frame_matrix,
            color_map=color_map,
            screen_x=x + offset_x_centering,
            screen_y=y + offset_y_centering,
            pixel_size_in_display=pixel_size_in_display,
        )

    def _render_single_item_detail(
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
        Renders the detailed view of a single selected item in the character view.
        """
        if (
            not self.selected_object_layer_id
            or self.selected_object_layer_id == EMPTY_SLOT_ID
        ):
            modal_component.set_title("No Item Selected")
            empty_text = "Select an equipped item for details."
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

        item_name = self.selected_object_layer_id.replace("_", " ").title()
        self.title_text = item_name
        modal_component.set_title(self.title_text)

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

        item_display_size = min(width, height) * 0.4  # Smaller for detail view
        matrix_dimension = self.object_layer_render.get_object_layer_matrix_dimension(
            self.selected_object_layer_id
        )
        pixel_size = item_display_size / matrix_dimension if matrix_dimension > 0 else 1

        centered_x = x + (width - item_display_size) / 2
        centered_y = y + 80  # Position below title

        anim_props = self.object_layer_render.get_or_create_object_layer_animation(
            obj_id=f"char_detail_item_{self.selected_object_layer_id}",
            object_layer_id=self.selected_object_layer_id,
            target_object_layer_size_pixels=pixel_size,
            initial_direction=Direction.DOWN,
        )
        anim_instance = anim_props["object_layer_animation_instance"]
        anim_instance.set_state(Direction.DOWN, ObjectLayerMode.WALKING, 0.0)

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

        description = (
            f"This is your equipped {item_name} for the {self.selected_slot_key} slot."
        )
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

        # Example: "Unequip" button
        button_width = 120
        button_height = 30
        button_x = x + (width - button_width) // 2
        button_y = height - button_height - 20

        button_color = Color(241, 0, 0, 200)  # Redish
        button_text = "Unequip"

        is_button_hovered = (
            mouse_x >= button_x
            and mouse_x <= (button_x + button_width)
            and mouse_y >= button_y
            and mouse_y <= (button_y + button_height)
        )
        if is_button_hovered:
            button_color.a = 255

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
        Main render method for the character view.
        Switches between hexagonal-slot layout and single item detail view.
        """
        if self.selected_slot_key is None:
            # Render the hexagonal-slot layout using GridCoreComponent
            self.title_text = "Character"
            modal_component.set_title(self.title_text)

            # Draw the title for the view
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

            # Calculate available vertical space for the grid (below title and padding)
            grid_top_y_after_title = y + title_font_size + 40
            available_height_for_grid = height - (grid_top_y_after_title - y)

            # Calculate vertical offset to center the grid within its available space
            total_grid_height = self.grid_component.total_grid_height
            vertical_offset_centering = (
                available_height_for_grid - total_grid_height
            ) // 2

            # Final Y offset for the grid (ensure it's not negative)
            grid_offset_y = grid_top_y_after_title + max(0, vertical_offset_centering)

            # Update the grid_slots_map to reflect current equipped items
            self.grid_slots_map = self._create_character_grid_map()

            # Pass the grid_slots_map as items_data. The _render_character_slot_item will use 'item_id' from it.
            self.grid_component.render(
                offset_x=x + self.grid_component.calculate_centered_offset_x(width),
                offset_y=grid_offset_y,
                container_width=width,
                container_height=height
                - (grid_offset_y - y),  # Pass the remaining height for the grid to use
                items_data=self.grid_slots_map,
                mouse_x=mouse_x,
                mouse_y=mouse_y,
                selected_key=self.selected_slot_key,  # Pass selected key for highlighting
            )

        else:
            # Render single item detail view
            self._render_single_item_detail(
                modal_component,
                x,
                y,
                width,
                height,
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
        mouse_x: int,
        mouse_y: int,
        is_mouse_button_pressed: bool,
    ) -> bool:
        """
        Handles clicks within the character view modal. Returns True if a click was handled.
        """
        if not is_mouse_button_pressed:
            return False

        if self.selected_slot_key is None:
            # In slot grid view, check for slot clicks using the grid component
            title_font_size = UI_FONT_SIZE + 2

            # Calculate grid_offset_y consistently with render_content
            grid_top_y_after_title = offset_y + title_font_size + 40
            total_grid_height = self.grid_component.total_grid_height
            available_height_for_grid = container_height - (
                grid_top_y_after_title - offset_y
            )
            vertical_offset_centering = (
                available_height_for_grid - total_grid_height
            ) // 2
            grid_offset_y = grid_top_y_after_title + max(0, vertical_offset_centering)

            clicked_index = self.grid_component.get_clicked_item_index(
                offset_x=offset_x
                + self.grid_component.calculate_centered_offset_x(container_width),
                offset_y=grid_offset_y,
                mouse_x=mouse_x,
                mouse_y=mouse_y,
                is_mouse_button_pressed=is_mouse_button_pressed,
            )

            if clicked_index is not None and clicked_index < len(self.grid_slots_map):
                clicked_item_data = self.grid_slots_map[clicked_index]
                if (
                    clicked_item_data["key"] is not None
                ):  # Ensure a valid slot was clicked (not a None placeholder)
                    self.selected_slot_key = clicked_item_data["key"]
                    self.selected_object_layer_id = self.equipped_object_layer_ids.get(
                        self.selected_slot_key, EMPTY_SLOT_ID
                    )
                    logging.info(
                        f"Selected character slot: {self.selected_slot_key}, Item: {self.selected_object_layer_id}"
                    )
                    return True
            return False
        else:
            # In single item detail view, check for "Unequip" button click
            button_width = 120
            button_height = 30
            button_x = offset_x + (container_width - button_width) // 2
            button_y = container_height - button_height - 20

            if (
                mouse_x >= button_x
                and mouse_x <= (button_x + button_width)
                and mouse_y >= button_y
                and mouse_y <= (button_y + button_height)
            ):
                logging.info(
                    f"Unequip button clicked for {self.selected_object_layer_id}"
                )
                # Placeholder for unequip logic (e.g., send to server, update equipped_object_layer_ids)
                if (
                    self.selected_slot_key and self.selected_slot_key != "center"
                ):  # Prevent unequipping the main body
                    self.equipped_object_layer_ids[self.selected_slot_key] = (
                        EMPTY_SLOT_ID
                    )
                self.reset_view()  # Go back to grid view after unequipping
                return True
            return False

    def reset_view(self):
        """Resets the character view to its initial grid state, clearing selection."""
        self.selected_slot_key = None
        self.selected_object_layer_id = None
        self.title_text = "Character"
        # Re-create grid map to reflect any unequipped items
        self.grid_slots_map = self._create_character_grid_map()
