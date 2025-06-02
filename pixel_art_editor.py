import numpy as np
import math  # Import math for parametric curves


class PixelArtEditor:
    """
    A class to manage and edit pixel art matrices.

    Attributes:
        matrix (np.array): The 2D NumPy array representing the pixel art.
        color_map (dict): A mapping from integer pixel values to RGBA color tuples (0-255).
        last_draw_color_id (int): Stores the color ID of the last drawing operation.
    """

    def __init__(self, initial_matrix, color_mapping):
        """
        Initializes the PixelArtEditor with an initial matrix and color mapping.

        Args:
            initial_matrix (list or np.array): The starting pixel art matrix.
            color_mapping (dict): A dictionary mapping integer pixel values to RGBA tuples.
        """
        self.matrix = np.array(initial_matrix)
        self.color_map = color_mapping
        self.last_draw_color_id = None  # To store the color of the last draw operation

    def rgba_to_mpl_color(self, r, g, b, a):
        """
        Converts RGBA color values from 0-255 range to 0-1 range for Matplotlib.

        Args:
            r (int): Red component (0-255).
            g (int): Green component (0-255).
            b (int): Blue component (0-255).
            a (int): Alpha component (0-255).

        Returns:
            tuple: A tuple (r, g, b, a) with values in the 0-1 range.
        """
        return (r / 255.0, g / 255.0, b / 255.0, a / 255.0)

    def get_mpl_color(self, pixel_value):
        """
        Retrieves the Matplotlib-compatible RGBA color for a given pixel value.

        Args:
            pixel_value (int): The integer value representing the color in the matrix.

        Returns:
            tuple: A tuple (r, g, b, a) with color values in the 0-1 range.
        """
        rgba_255 = self.color_map.get(
            pixel_value, (0, 0, 0, 0)
        )  # Default to transparent black
        return self.rgba_to_mpl_color(*rgba_255)

    def draw_pixel(self, row_idx, col_idx, color_id):
        """
        Draws a single pixel on the matrix with the specified color ID.
        This method directly overwrites the pixel at the given coordinates.

        Args:
            row_idx (int): The row index of the pixel.
            col_idx (int): The column index of the pixel.
            color_id (int): The integer ID of the color to draw.
        """
        # Ensure coordinates are within bounds before drawing
        if 0 <= row_idx < self.matrix.shape[0] and 0 <= col_idx < self.matrix.shape[1]:
            self.matrix[row_idx, col_idx] = color_id
            self.last_draw_color_id = color_id
        # else:
        # print(f"Warning: Attempted to draw outside matrix bounds at ({row_idx}, {col_idx})")

    def draw_rectangle(self, start_row, start_col, width, height, color_id):
        """
        Draws a filled rectangle on the matrix.

        Args:
            start_row (int): The starting row (top-left corner) of the rectangle.
            start_col (int): The starting column (top-left corner) of the rectangle.
            width (int): The width of the rectangle.
            height (int): The height of the rectangle.
            color_id (int): The integer ID of the color to draw the rectangle with.
        """
        end_row = start_row + height
        end_col = start_col + width

        # Iterate over the rectangle's area and draw each pixel
        for r in range(start_row, end_row):
            for c in range(start_col, end_col):
                self.draw_pixel(r, c, color_id)

    def draw_parametric_curve(
        self, x_func, y_func, t_start, t_end, num_points, color_id
    ):
        """
        Draws a parametric curve by plotting individual pixels.

        Args:
            x_func (callable): A function that returns the x-coordinate for a given parameter t.
            y_func (callable): A function that returns the y-coordinate for a given parameter t.
            t_start (float): The starting value of the parameter t.
            t_end (float): The ending value of the parameter t.
            num_points (int): The number of points to plot along the curve.
            color_id (int): The integer ID of the color to draw the curve with.
        """
        t_values = np.linspace(t_start, t_end, num_points)
        matrix_height, matrix_width = self.matrix.shape

        for t in t_values:
            # Calculate floating point coordinates
            x_float = x_func(t)
            y_float = y_func(t)

            # Convert to integer pixel coordinates, clamping to matrix bounds
            # We need to invert y for drawing to match matrix (0 at top) to plot (0 at bottom)
            col_idx = int(round(x_float))
            row_idx = int(
                round(y_float)
            )  # Matplotlib's y-axis is inverted relative to matrix rows

            self.draw_pixel(row_idx, col_idx, color_id)
