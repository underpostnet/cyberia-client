import logging
from raylibpy import (
    Color,
    draw_rectangle,
    draw_text,
)  # Import only necessary Raylib functions via RaylibManager

# --- Logging Configuration ---
logging.basicConfig(
    level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s"
)

# Assuming RaylibManager is available in core.raylib_manager
from core.raylib_manager import RaylibManager


class ItemRenderer:
    """
    Responsible for rendering individual animation frames (pixel art matrices)
    onto the screen using the RaylibManager. It does not manage animation state,
    only draws the current frame provided to it.
    """

    def __init__(self, raylib_manager: RaylibManager):
        """
        Initializes the ItemRenderer with a RaylibManager instance.

        Args:
            raylib_manager (RaylibManager): The centralized Raylib manager for drawing operations.
        """
        self.raylib_manager = raylib_manager
        logging.info("ItemRenderer initialized.")

    def render_animation_frame(
        self,
        frame_matrix: list[list[int]],
        color_map: list[Color],
        screen_x: float,
        screen_y: float,
        display_size_pixels: int,  # The target width/height in screen pixels
        base_color: Color,  # Fallback color for missing pixel colors
        flip_horizontal: bool = False,  # New parameter for horizontal flipping
        debug_frame_index: (
            int | None
        ) = None,  # Optional: for displaying frame index on sprite
    ):
        """
        Renders a single frame of an animation (a pixel art matrix) at the specified
        screen coordinates with scaling.

        Args:
            frame_matrix (list[list[int]]): The 2D integer matrix representing the current animation frame.
            color_map (list[Color]): A list of Raylib Color objects, where indices correspond to matrix values.
            screen_x (float): The X-coordinate on the screen where the top-left corner of the animation should be drawn.
            screen_y (float): The Y-coordinate on the screen where the top-left corner of the animation should be drawn.
            display_size_pixels (int): The desired total width and height of the rendered animation in pixels.
            base_color (Color): A fallback color to use if a matrix value's color is not found in the color_map.
            flip_horizontal (bool): If True, the sprite will be drawn flipped horizontally.
            debug_frame_index (int | None): If provided, this index will be drawn on the sprite for debugging.
        """
        matrix_dimension = len(frame_matrix)
        if matrix_dimension == 0:
            logging.warning("Render: Empty matrix provided. Cannot draw.")
            return

        # Calculate the size of each individual pixel in the scaled sprite.
        effective_pixel_size = max(1, int(display_size_pixels / matrix_dimension))

        for row_idx in range(matrix_dimension):
            for col_idx in range(matrix_dimension):
                matrix_value = frame_matrix[row_idx][col_idx]

                cell_color = None
                if matrix_value > 0 and matrix_value < len(color_map):
                    cell_color = color_map[matrix_value]
                elif matrix_value != 0:
                    # If matrix_value is 0, it means transparent/empty pixel, so no drawing
                    # Otherwise, use base_color for values not found in color_map
                    logging.debug(
                        f"Color mapping not found for matrix value: {matrix_value}. Using base_color."
                    )
                    cell_color = base_color

                if cell_color:
                    draw_col = col_idx
                    if flip_horizontal:
                        draw_col = matrix_dimension - 1 - col_idx

                    cell_draw_x = screen_x + draw_col * effective_pixel_size
                    cell_draw_y = screen_y + row_idx * effective_pixel_size

                    self.raylib_manager.draw_rectangle(
                        int(cell_draw_x),
                        int(cell_draw_y),
                        effective_pixel_size,
                        effective_pixel_size,
                        cell_color,
                    )

        # --- Visual Animation Indicator (for debugging/demo) ---
        # This is kept for the demo/dev purposes as per original items_engine.py
        indicator_size = int(effective_pixel_size * 2)
        indicator_x = int(screen_x)
        indicator_y = int(screen_y)

        indicator_colors = [
            Color(255, 0, 0, 255),
            Color(0, 255, 0, 255),
            Color(0, 0, 255, 255),
            Color(255, 255, 0, 255),
        ]

        # Use a consistent indicator color based on the debug frame index if provided,
        # otherwise, default to using the first color.
        if debug_frame_index is not None:
            current_indicator_color = indicator_colors[
                debug_frame_index % len(indicator_colors)
            ]
        else:
            current_indicator_color = indicator_colors[0]  # Default if no debug index

        self.raylib_manager.draw_rectangle(
            indicator_x,
            indicator_y,
            indicator_size,
            indicator_size,
            current_indicator_color,
        )

        # --- On-Sprite Frame Index Display (for debugging/demo) ---
        if debug_frame_index is not None:
            text_size = int(display_size_pixels * 0.15)
            if text_size < 10:
                text_size = 10

            frame_text = str(debug_frame_index)
            text_x = int(screen_x + effective_pixel_size)
            text_y = int(screen_y + effective_pixel_size)
            self.raylib_manager.draw_text(
                frame_text, text_x, text_y, text_size, Color(255, 255, 255, 255)
            )
