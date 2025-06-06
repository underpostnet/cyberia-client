import logging
from raylibpy import (
    Color,
    Rectangle,
    Vector2,
    draw_rectangle_lines,
    draw_text,
    measure_text as raylib_measure_text,
)

logging.basicConfig(
    level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s"
)


class GridCoreComponent:
    """
    A UI component for rendering a flexible grid of items.
    It can be used for inventories, quest lists, or any other grid-based display.
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
    ):
        self.object_layer_render = object_layer_render_instance
        self.num_rows = num_rows
        self.num_cols = num_cols
        self.item_width = item_width
        self.item_height = item_height
        self.item_padding = item_padding
        self.render_item_callback = render_item_callback
        self.background_color = background_color
        self.border_color = border_color
        self.slot_background_color = slot_background_color
        self.slot_hover_color = slot_hover_color
        self.slot_selected_color = slot_selected_color

        self.total_grid_width = self._calculate_total_grid_width()
        self.total_grid_height = self._calculate_total_grid_height()

    def _calculate_total_grid_width(self) -> int:
        """Calculates the total width of the grid."""
        if self.num_cols == 0:
            return 0
        return self.num_cols * self.item_width + (self.num_cols - 1) * self.item_padding

    def _calculate_total_grid_height(self) -> int:
        """Calculates the total height of the grid."""
        if self.num_rows == 0:
            return 0
        return (
            self.num_rows * self.item_height + (self.num_rows - 1) * self.item_padding
        )

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
    ):
        """
        Renders the grid within the given container bounds, with items.

        Args:
            offset_x: X offset of the container.
            offset_y: Y offset of the container.
            container_width: Width of the area where the grid will be drawn.
            container_height: Height of the area where the grid will be drawn.
            items_data: A list of data for each item to be rendered in the grid.
                        The order corresponds to row-major order (left to right, top to bottom).
            mouse_x: Current X coordinate of the mouse.
            mouse_y: Current Y coordinate of the mouse.
            selected_index: The index of the currently selected item (if any) in items_data.
        """
        # Draw background for the grid itself (optional, if container doesn't draw it)
        # self.object_layer_render.draw_rectangle(
        #     offset_x, offset_y, container_width, container_height, self.background_color
        # )

        current_item_index = 0
        for row in range(self.num_rows):
            for col in range(self.num_cols):
                slot_x = offset_x + col * (self.item_width + self.item_padding)
                slot_y = offset_y + row * (self.item_height + self.item_padding)

                # Check if this slot is within the valid drawing area of the container
                if (
                    slot_x + self.item_width > offset_x
                    and slot_y + self.item_height > offset_y
                    and slot_x < offset_x + container_width
                    and slot_y < offset_y + container_height
                ):
                    # Determine slot background color based on hover and selection state
                    is_hovered = (
                        mouse_x >= slot_x
                        and mouse_x <= (slot_x + self.item_width)
                        and mouse_y >= slot_y
                        and mouse_y <= (slot_y + self.item_height)
                    )
                    is_selected = current_item_index == selected_index

                    slot_bg_color = self.slot_background_color
                    if is_selected:
                        slot_bg_color = self.slot_selected_color
                    elif is_hovered:
                        slot_bg_color = self.slot_hover_color

                    self.object_layer_render.draw_rectangle(
                        slot_x, slot_y, self.item_width, self.item_height, slot_bg_color
                    )
                    draw_rectangle_lines(
                        slot_x,
                        slot_y,
                        self.item_width,
                        self.item_height,
                        self.border_color,
                    )

                    # Render item content if available
                    if items_data and current_item_index < len(items_data):
                        item_data = items_data[current_item_index]
                        if self.render_item_callback:
                            self.render_item_callback(
                                self.object_layer_render,
                                slot_x,
                                slot_y,
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
                            text_x = slot_x + (self.item_width - text_width) // 2
                            text_y = slot_y + (self.item_height - 16) // 2
                            draw_text(
                                placeholder_text,
                                text_x,
                                text_y,
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
                slot_x = offset_x + col * (self.item_width + self.item_padding)
                slot_y = offset_y + row * (self.item_height + self.item_padding)

                # Check if mouse is within this slot's bounds
                if (
                    mouse_x >= slot_x
                    and mouse_x <= (slot_x + self.item_width)
                    and mouse_y >= slot_y
                    and mouse_y <= (slot_y + self.item_height)
                ):
                    clicked_index = row * self.num_cols + col
                    logging.debug(f"Grid item clicked: {clicked_index}")
                    return clicked_index
        return None
