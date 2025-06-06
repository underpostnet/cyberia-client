import logging
import math
from raylibpy import (
    Color,
    Vector2,
    draw_rectangle,
    draw_rectangle_lines,
    draw_text,
    measure_text as raylib_measure_text,
    draw_poly,  # New import for drawing hexagons
)

logging.basicConfig(
    level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s"
)


class GridCoreComponent:
    """
    A UI component for rendering a flexible grid of items, supporting
    rectangular and visually hexagonal layouts.
    """

    def __init__(
        self,
        object_layer_render_instance,
        num_rows: int,
        num_cols: int,
        item_width: int,
        item_height: int,
        item_padding: int = 5,
        render_item_callback=None,
        background_color: Color = Color(0, 0, 0, 150),
        border_color: Color = Color(100, 100, 100, 200),
        slot_background_color: Color = Color(50, 50, 50, 180),
        slot_hover_color: Color = Color(70, 70, 70, 200),
        slot_selected_color: Color = Color(
            200, 200, 0, 200
        ),  # Default yellow for selection
        grid_type: str = "rectangle",  # 'rectangle' or 'hexagon'
        hexagon_radius: int = 0,  # Required for 'hexagon' type, defines the visual size of the hex
    ):
        self.object_layer_render = object_layer_render_instance
        self.num_rows = num_rows
        self.num_cols = num_cols
        self.item_width = item_width  # This is now the content area size within a slot
        self.item_height = (
            item_height  # This is now the content area size within a slot
        )
        self.item_padding = item_padding
        self.render_item_callback = render_item_callback
        self.background_color = background_color
        self.border_color = border_color
        self.slot_background_color = slot_background_color
        self.slot_hover_color = slot_hover_color
        self.slot_selected_color = slot_selected_color
        self.grid_type = grid_type
        self.hexagon_radius = hexagon_radius

        if self.grid_type == "hexagon" and self.hexagon_radius <= 0:
            logging.warning(
                "Hexagon grid type requires a positive hexagon_radius. Setting to default."
            )
            self.hexagon_radius = 40  # Default if not set for hexagons

        self.total_grid_width = self._calculate_total_grid_width()
        self.total_grid_height = self._calculate_total_grid_height()

        # Vertices for a pointy-top hexagon, relative to its center, used for drawing
        self._hexagon_vertices = self._get_hexagon_vertices(self.hexagon_radius)

    def _get_hexagon_vertices(self, radius: int) -> list[Vector2]:
        """Calculates vertices for a pointy-top hexagon centered at (0,0)."""
        vertices = []
        for i in range(6):
            angle_deg = 30 + i * 60  # Start from 30deg for pointy top
            angle_rad = math.radians(angle_deg)
            x = radius * math.cos(angle_rad)
            y = radius * math.sin(angle_rad)
            vertices.append(Vector2(x, y))
        return vertices

    def _get_hexagon_slot_bounding_box(
        self, offset_x: int, offset_y: int, row: int, col: int
    ) -> tuple[float, float, float, float]:
        """
        Calculates the bounding box (x, y, width, height) for a hexagonal slot.
        This bounding box is used for simplified click detection.
        """
        R = self.hexagon_radius

        # Horizontal spacing for hexagonal grid (distance between centers horizontally)
        col_spacing = R * 1.5
        # Vertical spacing for hexagonal grid (distance between centers vertically)
        row_spacing = R * math.sqrt(3)

        # Calculate nominal center based on row/col
        center_x = offset_x + col * col_spacing
        center_y = offset_y + row * row_spacing

        # Apply staggering for odd columns
        if col % 2 == 1:
            center_y += R * math.sqrt(3) / 2

        # Bounding box dimensions for a pointy-top hexagon
        bbox_width = R * math.sqrt(3)  # distance between two parallel sides
        bbox_height = 2 * R  # distance between two opposite points

        # Calculate top-left corner of the bounding box
        slot_x = center_x - bbox_width / 2
        slot_y = center_y - bbox_height / 2

        return slot_x, slot_y, bbox_width, bbox_height

    def _calculate_total_grid_width(self) -> int:
        """Calculates the total width of the grid based on its type."""
        if self.num_cols == 0:
            return 0

        if self.grid_type == "rectangle":
            return (
                self.num_cols * self.item_width
                + (self.num_cols - 1) * self.item_padding
            )
        elif self.grid_type == "hexagon":
            R = self.hexagon_radius
            bbox_width = R * math.sqrt(3)
            col_spacing = R * 1.5
            return int(
                (self.num_cols - 1) * col_spacing + bbox_width + self.item_padding
            )  # Add padding for edges
        return 0

    def _calculate_total_grid_height(self) -> int:
        """Calculates the total height of the grid based on its type."""
        if self.num_rows == 0:
            return 0

        if self.grid_type == "rectangle":
            return (
                self.num_rows * self.item_height
                + (self.num_rows - 1) * self.item_padding
            )
        elif self.grid_type == "hexagon":
            R = self.hexagon_radius
            bbox_height = 2 * R
            row_spacing = R * math.sqrt(3)
            stagger_offset = (
                R * math.sqrt(3) / 2 if self.num_cols > 1 else 0
            )  # Account for staggering
            return int(
                (self.num_rows - 1) * row_spacing
                + bbox_height
                + stagger_offset
                + self.item_padding
            )  # Add padding for edges
        return 0

    def update_grid_dimensions(self, new_rows: int = None, new_cols: int = None):
        """Updates the grid dimensions dynamically, recalculating total size."""
        if new_rows is not None:
            self.num_rows = new_rows
        if new_cols is not None:
            self.num_cols = new_cols

        self.total_grid_width = self._calculate_total_grid_width()
        self.total_grid_height = self._calculate_total_grid_height()

    def calculate_centered_offset_x(self, container_width: int) -> int:
        """
        Calculates the X offset needed to center the grid horizontally within a container.

        Args:
            container_width: The width of the container.

        Returns:
            The X offset for centering.
        """
        return (container_width - self.total_grid_width) // 2

    def render(
        self,
        offset_x: int,
        offset_y: int,
        container_width: int,
        container_height: int,
        items_data: list = None,
        mouse_x: int = -1,
        mouse_y: int = -1,
        selected_index: int | None = None,
        selected_key: str | None = None,  # Used for character view to highlight by key
    ):
        """
        Renders the grid within the given container bounds, with items.
        """
        current_item_index = 0
        for row in range(self.num_rows):
            for col in range(self.num_cols):
                slot_draw_x, slot_draw_y, slot_width, slot_height = 0, 0, 0, 0
                slot_center_x, slot_center_y = 0, 0

                if self.grid_type == "rectangle":
                    slot_draw_x = offset_x + col * (self.item_width + self.item_padding)
                    slot_draw_y = offset_y + row * (
                        self.item_height + self.item_padding
                    )
                    slot_width = self.item_width
                    slot_height = self.item_height
                    slot_center_x = slot_draw_x + slot_width // 2
                    slot_center_y = slot_draw_y + slot_height // 2

                    # Check if this slot is within the valid drawing area of the container
                    if not (
                        slot_draw_x + slot_width > offset_x
                        and slot_draw_y + slot_height > offset_y
                        and slot_draw_x < offset_x + container_width
                        and slot_draw_y < offset_y + container_height
                    ):
                        current_item_index += 1
                        continue  # Skip drawing if outside container

                elif self.grid_type == "hexagon":
                    # Get the bounding box for click detection
                    slot_draw_x, slot_draw_y, slot_width, slot_height = (
                        self._get_hexagon_slot_bounding_box(
                            offset_x, offset_y, row, col
                        )
                    )

                    # Calculate center for drawing the hexagon
                    R = self.hexagon_radius
                    col_spacing = R * 1.5
                    row_spacing = R * math.sqrt(3)
                    slot_center_x = offset_x + col * col_spacing
                    slot_center_y = offset_y + row * row_spacing
                    if col % 2 == 1:
                        slot_center_y += R * math.sqrt(3) / 2

                    # Also need to check if the *content* should be rendered here, not just the bounding box
                    # This implies `items_data` needs to handle `None` entries for empty slots.
                    if items_data and current_item_index < len(items_data):
                        if items_data[current_item_index].get("key") is None:
                            current_item_index += 1
                            continue  # Skip drawing this "empty" grid cell
                    else:
                        current_item_index += 1
                        continue  # Skip if no item data for this index

                is_hovered = (
                    mouse_x >= slot_draw_x
                    and mouse_x <= (slot_draw_x + slot_width)
                    and mouse_y >= slot_draw_y
                    and mouse_y <= (slot_draw_y + slot_height)
                )

                is_selected = False
                if items_data and current_item_index < len(items_data):
                    # Check selection by index (general grid) or by key (character view)
                    if (
                        selected_index is not None
                        and current_item_index == selected_index
                    ):
                        is_selected = True
                    elif (
                        selected_key is not None
                        and items_data[current_item_index].get("key") == selected_key
                    ):
                        is_selected = True

                slot_bg_color = self.slot_background_color
                if is_selected:
                    slot_bg_color = self.slot_selected_color
                elif is_hovered:
                    slot_bg_color = self.slot_hover_color

                if self.grid_type == "rectangle":
                    self.object_layer_render.draw_rectangle(
                        int(slot_draw_x),
                        int(slot_draw_y),
                        int(slot_width),
                        int(slot_height),
                        slot_bg_color,
                    )
                    draw_rectangle_lines(
                        int(slot_draw_x),
                        int(slot_draw_y),
                        int(slot_width),
                        int(slot_height),
                        self.border_color,
                    )
                elif self.grid_type == "hexagon":
                    # Draw the hexagon shape itself
                    draw_poly(
                        Vector2(slot_center_x, slot_center_y),
                        self.hexagon_radius,
                        6,
                        30,
                        slot_bg_color,
                    )

                    # Draw hexagon border
                    border_vertices = [
                        Vector2(v.x + slot_center_x, v.y + slot_center_y)
                        for v in self._hexagon_vertices
                    ]
                    for i in range(6):
                        p1 = border_vertices[i]
                        p2 = border_vertices[(i + 1) % 6]
                        self.object_layer_render.draw_line(
                            int(p1.x),
                            int(p1.y),
                            int(p2.x),
                            int(p2.y),
                            self.border_color,
                        )

                # Render item content if available
                if items_data and current_item_index < len(items_data):
                    item_data = items_data[current_item_index]
                    if self.render_item_callback:
                        # Pass the content area size (item_width/height) for rendering item,
                        # centered within the larger bounding box of the slot.
                        content_x = slot_draw_x + (slot_width - self.item_width) / 2
                        content_y = slot_draw_y + (slot_height - self.item_height) / 2
                        self.render_item_callback(
                            self.object_layer_render,
                            int(content_x),
                            int(content_y),
                            self.item_width,
                            self.item_height,
                            item_data,
                            is_hovered,
                            is_selected,
                        )
                    else:
                        # Fallback for empty callback: draw placeholder text
                        placeholder_text = f"Item {current_item_index}"
                        text_width = raylib_measure_text(
                            placeholder_text, 16
                        )  # Small font size
                        text_x = slot_draw_x + (slot_width - text_width) // 2
                        text_y = slot_draw_y + (slot_height - 16) // 2
                        draw_text(
                            placeholder_text,
                            int(text_x),
                            int(text_y),
                            16,
                            Color(200, 200, 200, 255),
                        )
                current_item_index += 1

    def get_clicked_item_index(
        self,
        offset_x: int,
        offset_y: int,
        mouse_x: int,
        mouse_y: int,
        is_mouse_button_pressed: bool,
    ) -> int | None:
        """
        Determines if an item in the grid was clicked.
        For hexagons, this checks against the rectangular bounding box for simplicity.

        Args:
            offset_x: X offset of the container.
            offset_y: Y offset of the container.
            mouse_x: Current X coordinate of the mouse.
            mouse_y: Current Y coordinate of the mouse.
            is_mouse_button_pressed: True if the mouse button is pressed.

        Returns:
            The index of the clicked item (0-indexed, row-major) or None if no item was clicked.
        """
        if not is_mouse_button_pressed:
            return None

        for row in range(self.num_rows):
            for col in range(self.num_cols):
                slot_x, slot_y, slot_width, slot_height = 0, 0, 0, 0
                if self.grid_type == "rectangle":
                    slot_x = offset_x + col * (self.item_width + self.item_padding)
                    slot_y = offset_y + row * (self.item_height + self.item_padding)
                    slot_width = self.item_width
                    slot_height = self.item_height
                elif self.grid_type == "hexagon":
                    slot_x, slot_y, slot_width, slot_height = (
                        self._get_hexagon_slot_bounding_box(
                            offset_x, offset_y, row, col
                        )
                    )

                # Check if mouse is within this slot's bounding box
                if (
                    mouse_x >= slot_x
                    and mouse_x <= (slot_x + slot_width)
                    and mouse_y >= slot_y
                    and mouse_y <= (slot_y + slot_height)
                ):
                    clicked_index = row * self.num_cols + col
                    logging.debug(f"Grid item clicked: {clicked_index}")
                    return clicked_index
        return None
