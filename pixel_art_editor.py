import numpy as np

# collections is no longer needed as flood_fill is removed
# import collections


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
        if 0 <= row_idx < self.matrix.shape[0] and 0 <= col_idx < self.matrix.shape[1]:
            self.matrix[row_idx, col_idx] = color_id
            self.last_draw_color_id = color_id
        else:
            print(
                f"Warning: Attempted to draw outside matrix bounds at ({row_idx}, {col_idx})"
            )
