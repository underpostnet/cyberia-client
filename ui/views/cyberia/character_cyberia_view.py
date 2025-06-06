import logging
import math
from raylibpy import Color, Vector2, RAYWHITE
from config import (
    UI_FONT_SIZE,
    UI_TEXT_COLOR_PRIMARY,
    UI_TEXT_COLOR_SHADING,
)
from object_layer.object_layer_render import ObjectLayerRender
from object_layer.object_layer_data import (
    Direction,
    ObjectLayerMode,
)

logging.basicConfig(
    level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s"
)

# Constants for the character view layout
CHARACTER_SLOT_SIZE = 60  # Size of each hexagonal slot
CHARACTER_HEXAGON_RADIUS = 80  # Distance from center to the middle of outer slots
CHARACTER_MAX_HEIGHT_CONSTRAINT = 300  # Max height for the hexagon container

# Placeholder for empty slot in character view
EMPTY_SLOT_ID = "EMPTY_SLOT"  # Using a placeholder for now


class CharacterCyberiaView:
    """
    Manages the display and interaction for the player's character equipment interface.
    Displays 7 slots in a hexagonal layout: one center, six around it.
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
        self.slot_order = [
            "head",
            "weapon",
            "chest",
            "hands",
            "legs",
            "feet",
        ]  # Clockwise from top-left

        # Track selected item for detail view
        self.selected_slot_key: str | None = None
        self.selected_object_layer_id: str | None = None

        # Calculate hexagonal slot positions relative to the hexagon's center
        self._calculate_slot_positions()

    def _calculate_slot_positions(self):
        """
        Calculates the relative (x, y) coordinates for each of the 7 hexagonal slots.
        """
        self.slot_positions = {}

        # Center slot
        self.slot_positions["center"] = Vector2(0, 0)

        # Outer 6 slots
        for i, key in enumerate(self.slot_order):
            angle_deg = 90 + (i * 60)  # Start from top (90 deg), then 60 deg increments
            angle_rad = math.radians(angle_deg)
            x = CHARACTER_HEXAGON_RADIUS * math.cos(angle_rad)
            y = CHARACTER_HEXAGON_RADIUS * math.sin(angle_rad)
            self.slot_positions[key] = Vector2(x, y)

    def _render_character_slot_item(
        self,
        object_layer_render_instance: ObjectLayerRender,
        x: int,
        y: int,
        width: int,
        height: int,
        item_object_layer_id: str,  # item_data is the object_layer_id
        is_hovered: bool,
        is_selected: bool,
    ):
        """
        Renders a single character slot item.
        Similar to bag slot rendering, it displays the object layer animation.
        """
        # Ensure item_object_layer_id is valid for rendering, otherwise use placeholder
        if (
            item_object_layer_id == EMPTY_SLOT_ID
            or not object_layer_render_instance.get_object_layer_definition(
                item_object_layer_id
            )
        ):
            placeholder_text = (
                item_object_layer_id.replace("_", " ").title()
                if item_object_layer_id
                else "Empty"
            )
            text_width = object_layer_render_instance.measure_text(
                placeholder_text, UI_FONT_SIZE - 4
            )
            text_x = x + (width - text_width) // 2
            text_y = y + (height - (UI_FONT_SIZE - 4)) // 2

            object_layer_render_instance.draw_text(
                placeholder_text,
                text_x + 1,
                text_y + 1,
                UI_FONT_SIZE - 4,
                Color(*UI_TEXT_COLOR_SHADING),
            )
            object_layer_render_instance.draw_text(
                placeholder_text,
                text_x,
                text_y,
                UI_FONT_SIZE - 4,
                Color(*UI_TEXT_COLOR_PRIMARY),
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

        pixel_size_in_display = width // matrix_dimension
        if pixel_size_in_display == 0:
            pixel_size_in_display = 1

        rendered_width = pixel_size_in_display * matrix_dimension
        rendered_height = pixel_size_in_display * matrix_dimension

        offset_x_centering = (width - rendered_width) // 2
        offset_y_centering = (height - rendered_height) // 2

        # Get or create the animation instance
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

        self.object_layer_render.render_specific_object_layer_frame(
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
        Switches between hexagon layout and single item detail view.
        """
        if self.selected_slot_key is None:
            # Render the hexagonal layout
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

            # Calculate the center of the hexagon relative to the modal content area
            # This accounts for the title and padding to keep hexagon within MAX_HEIGHT_CONSTRAINT
            available_height = CHARACTER_MAX_HEIGHT_CONSTRAINT - (title_y - y)
            if (
                available_height < CHARACTER_SLOT_SIZE * 2
            ):  # Ensure enough space for slots
                available_height = CHARACTER_SLOT_SIZE * 2

            hexagon_center_x = x + width // 2
            hexagon_center_y = (
                y + (title_y - y) + available_height // 2 + 20
            )  # Offset below title

            # Render each slot
            all_slots = {"center": self.equipped_object_layer_ids["center"]}
            all_slots.update(
                {k: self.equipped_object_layer_ids[k] for k in self.slot_order}
            )

            for slot_key, rel_pos in self.slot_positions.items():
                slot_x = hexagon_center_x + rel_pos.x - CHARACTER_SLOT_SIZE // 2
                slot_y = hexagon_center_y + rel_pos.y - CHARACTER_SLOT_SIZE // 2

                # Check for hover state
                is_hovered = (
                    mouse_x >= slot_x
                    and mouse_x <= (slot_x + CHARACTER_SLOT_SIZE)
                    and mouse_y >= slot_y
                    and mouse_y <= (slot_y + CHARACTER_SLOT_SIZE)
                )
                is_selected = self.selected_slot_key == slot_key

                # Determine slot background color based on hover and selection state
                slot_bg_color = Color(30, 30, 30, 180)  # Default
                if is_selected:
                    slot_bg_color = Color(100, 100, 0, 200)  # Selected color
                elif is_hovered:
                    slot_bg_color = Color(50, 50, 50, 200)  # Hover color

                self.object_layer_render.draw_rectangle(
                    slot_x,
                    slot_y,
                    CHARACTER_SLOT_SIZE,
                    CHARACTER_SLOT_SIZE,
                    slot_bg_color,
                )
                self.object_layer_render.draw_rectangle_lines(
                    slot_x,
                    slot_y,
                    CHARACTER_SLOT_SIZE,
                    CHARACTER_SLOT_SIZE,
                    Color(50, 50, 50, 200),  # Border
                )

                # Render item content
                item_id = all_slots.get(slot_key, EMPTY_SLOT_ID)
                self._render_character_slot_item(
                    self.object_layer_render,
                    slot_x,
                    slot_y,
                    CHARACTER_SLOT_SIZE,
                    CHARACTER_SLOT_SIZE,
                    item_id,
                    is_hovered,
                    is_selected,
                )

                # Draw slot name
                slot_name_text = slot_key.replace("_", " ").title()
                text_width = self.object_layer_render.measure_text(
                    slot_name_text, UI_FONT_SIZE - 5
                )
                text_x = slot_x + (CHARACTER_SLOT_SIZE - text_width) // 2
                text_y = slot_y + CHARACTER_SLOT_SIZE + 2  # A little below the slot

                self.object_layer_render.draw_text(
                    slot_name_text,
                    text_x + 1,
                    text_y + 1,
                    UI_FONT_SIZE - 5,
                    Color(*UI_TEXT_COLOR_SHADING),
                )
                self.object_layer_render.draw_text(
                    slot_name_text,
                    text_x,
                    text_y,
                    UI_FONT_SIZE - 5,
                    Color(*UI_TEXT_COLOR_PRIMARY),
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
            # In hexagon view, check for slot clicks
            title_font_size = UI_FONT_SIZE + 2
            available_height = CHARACTER_MAX_HEIGHT_CONSTRAINT - (
                20
            )  # Approx title height + padding
            if available_height < CHARACTER_SLOT_SIZE * 2:
                available_height = CHARACTER_SLOT_SIZE * 2

            hexagon_center_x = offset_x + container_width // 2
            hexagon_center_y = (
                offset_y + (20) + available_height // 2 + 20
            )  # Offset below title

            for slot_key, rel_pos in self.slot_positions.items():
                slot_x = hexagon_center_x + rel_pos.x - CHARACTER_SLOT_SIZE // 2
                slot_y = hexagon_center_y + rel_pos.y - CHARACTER_SLOT_SIZE // 2

                if (
                    mouse_x >= slot_x
                    and mouse_x <= (slot_x + CHARACTER_SLOT_SIZE)
                    and mouse_y >= slot_y
                    and mouse_y <= (slot_y + CHARACTER_SLOT_SIZE)
                ):
                    self.selected_slot_key = slot_key
                    self.selected_object_layer_id = self.equipped_object_layer_ids.get(
                        slot_key, EMPTY_SLOT_ID
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
                self.equipped_object_layer_ids[self.selected_slot_key] = EMPTY_SLOT_ID
                self.reset_view()  # Go back to grid view after unequipping
                return True
            return False

    def reset_view(self):
        """Resets the character view to its initial hexagon grid state, clearing selection."""
        self.selected_slot_key = None
        self.selected_object_layer_id = None
        self.title_text = "Character"
