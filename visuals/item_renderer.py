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
        display_size_pixels: int,
        flip_horizontal: bool = False,
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
            flip_horizontal (bool): If True, the sprite will be drawn flipped horizontally.
            debug_frame_index (int | None): If provided, this index will be drawn on the sprite for debugging.
        """
        matrix_dimension = len(frame_matrix)
        if matrix_dimension == 0:
            logging.warning("Render: Empty matrix provided. Cannot draw.")
            return

        for row_idx in range(matrix_dimension):
            for col_idx in range(matrix_dimension):
                matrix_value = frame_matrix[row_idx][col_idx]

                draw_col = col_idx
                if flip_horizontal:
                    draw_col = matrix_dimension - 1 - col_idx
                cell_draw_x = screen_x + draw_col * display_size_pixels
                cell_draw_y = screen_y + row_idx * display_size_pixels
                self.raylib_manager.draw_rectangle(
                    cell_draw_x,
                    cell_draw_y,
                    display_size_pixels,
                    display_size_pixels,
                    color_map[matrix_value],
                )
