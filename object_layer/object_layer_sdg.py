import numpy as np
import math  # Import math for parametric curves
import collections  # Import collections for deque used in contiguous_region_fill


class SyntheticDataGenerator:
    """
    A class to manage and generate synthetic data matrices.

    Attributes:
        data_matrix (np.array): The 2D NumPy array representing the synthetic data.
        value_map (dict): A mapping from integer data values to RGBA color tuples (0-255)
                          for display purposes.
        last_generated_value_id (int): Stores the value ID of the last data generation operation.
    """

    def __init__(self, initial_data_matrix, value_mapping):
        """
        Initializes the SyntheticDataGenerator with an initial data matrix and value mapping.

        Args:
            initial_data_matrix (list or np.array): The starting synthetic data matrix.
            value_mapping (dict): A dictionary mapping integer data values to RGBA tuples.
        """
        self.data_matrix = np.array(initial_data_matrix)
        self.value_map = value_mapping
        self.last_generated_value_id = (
            None  # To store the value of the last generation operation
        )

    def rgba_to_display_color(self, r, g, b, a):
        """
        Converts RGBA color values from 0-255 range to 0-1 range for Matplotlib display.

        Args:
            r (int): Red component (0-255).
            g (int): Green component (0-255).
            b (int): Blue component (0-255).
            a (int): Alpha component (0-255).

        Returns:
            tuple: A tuple (r, g, b, a) with values in the 0-1 range.
        """
        return (r / 255.0, g / 255.0, b / 255.0, a / 255.0)

    def get_display_color(self, data_value):
        """
        Retrieves the Matplotlib-compatible RGBA color for a given data value.

        Args:
            data_value (int): The integer value representing the data in the matrix.

        Returns:
            tuple: A tuple (r, g, b, a) with color values in the 0-1 range.
        """
        rgba_255 = self.value_map.get(
            data_value, (0, 0, 0, 0)
        )  # Default to transparent black
        return self.rgba_to_display_color(*rgba_255)

    def _convert_coordinates_to_indices(self, x, y):
        """
        Converts (x, y) coordinates (where (0,0) is bottom-left) to
        (row_idx, col_idx) for the internal data matrix (where (0,0) is top-left).
        Also clamps coordinates to be within matrix bounds.
        """
        matrix_height, matrix_width = self.data_matrix.shape

        # Invert y to get row_idx (0 at top)
        row_idx = matrix_height - 1 - y
        col_idx = x

        # Clamp coordinates to ensure they are within the matrix bounds
        clamped_row_idx = max(0, min(row_idx, matrix_height - 1))
        clamped_col_idx = max(0, min(col_idx, matrix_width - 1))
        return clamped_row_idx, clamped_col_idx

    def generate_data_point(self, x, y, value_id):
        """
        Generates a single data point on the matrix with the specified value ID.
        This method directly overwrites the data point at the given (x, y) coordinates,
        where (0,0) is considered the bottom-left of the data matrix.

        Args:
            x (int): The x-coordinate (column) of the data point.
            y (int): The y-coordinate (row) of the data point.
            value_id (int): The integer ID of the value to generate.
        """
        row_idx, col_idx = self._convert_coordinates_to_indices(x, y)
        self.data_matrix[row_idx, col_idx] = value_id
        self.last_generated_value_id = value_id

    def generate_rectangular_region(self, start_x, start_y, width, height, value_id):
        """
        Generates a filled rectangular region on the data matrix.
        The rectangle is defined by its bottom-left corner (start_x, start_y),
        width, and height.

        Args:
            start_x (int): The starting x-coordinate (bottom-left corner) of the rectangle.
            start_y (int): The starting y-coordinate (bottom-left corner) of the rectangle.
            width (int): The width of the rectangle.
            height (int): The height of the rectangle.
            value_id (int): The integer ID of the value to fill the rectangle with.
        """
        # Calculate the top-right corner in (x,y) coordinates
        end_x = start_x + width
        end_y = start_y + height

        # Iterate over the rectangle's area and generate each data point
        # We iterate over x from start_x to end_x-1, and y from start_y to end_y-1
        for x in range(start_x, end_x):
            for y in range(start_y, end_y):
                self.generate_data_point(x, y, value_id)

    def generate_parametric_curve_data(
        self, x_func, y_func, t_start, t_end, num_points, value_id
    ):
        """
        Generates data points along a parametric curve.
        The x_func and y_func should return coordinates where (0,0) is bottom-left.

        Args:
            x_func (callable): A function that returns the x-coordinate for a given parameter t.
            y_func (callable): A function that returns the y-coordinate for a given parameter t.
            t_start (float): The starting value of the parameter t.
            t_end (float): The ending value of the parameter t.
            num_points (int): The number of points to generate along the curve.
            value_id (int): The integer ID of the value to generate for the curve.
        """
        t_values = np.linspace(t_start, t_end, num_points)

        for t in t_values:
            # Calculate floating point coordinates (x, y) where (0,0) is bottom-left
            x_float = x_func(t)
            y_float = y_func(t)

            # Convert to integer data point coordinates
            x_int = int(round(x_float))
            y_int = int(round(y_float))

            self.generate_data_point(x_int, y_int, value_id)

    def get_coordinates_in_region(self, x1, y1, x2, y2):
        """
        Returns a list of all integer (x, y) coordinates within a rectangular region
        defined by two (x,y) input coordinates (where (0,0) is bottom-left).
        The coordinates are clamped to the data matrix bounds.

        Args:
            x1 (int): X-coordinate of the first point.
            y1 (int): Y-coordinate of the first point.
            x2 (int): X-coordinate of the second point.
            y2 (int): Y-coordinate of the second point.

        Returns:
            list: A list of (x, y) tuples representing all integer coordinates
                  within the specified area and clamped to the matrix boundaries.
        """
        # Determine the min and max x and y values to define the bounding box
        min_x = min(x1, x2)
        max_x = max(x1, x2)
        min_y = min(y1, y2)
        max_y = max(y1, y2)

        coordinates = []
        matrix_height, matrix_width = self.data_matrix.shape

        # Iterate through the bounding box, clamping to matrix limits
        for y in range(min_y, max_y + 1):
            for x in range(min_x, max_x + 1):
                # Clamp (x,y) coordinates to ensure they are within the plotting area
                clamped_x = max(0, min(x, matrix_width - 1))
                clamped_y = max(0, min(y, matrix_height - 1))
                coordinates.append((clamped_x, clamped_y))

        # Using a set for uniqueness and then converting back to a list
        return list(set(coordinates))

    def contiguous_region_fill(self, start_x, start_y, fill_value_id=0):
        """
        Performs a contiguous region fill operation starting from a given (x, y) coordinate
        (where (0,0) is bottom-left).
        Changes the value of adjacent data points to 'fill_value_id' (defaulting to 0/white)
        if their original value matches the starting data point's value, until a different
        value is found.

        Args:
            start_x (int): The starting x-coordinate for the fill.
            start_y (int): The starting y-coordinate for the fill.
            fill_value_id (int): The value ID to fill with. Defaults to 0 (white).
        """
        # Convert start_x, start_y to internal matrix row_idx, col_idx
        start_row, start_col = self._convert_coordinates_to_indices(start_x, start_y)

        # Check bounds for the internal matrix coordinates
        if not (
            0 <= start_row < self.data_matrix.shape[0]
            and 0 <= start_col < self.data_matrix.shape[1]
        ):
            print(
                f"Error: Start coordinates ({start_x}, {start_y}) are out of bounds for region fill."
            )
            return

        original_value_id = self.data_matrix[start_row, start_col]

        # If the original value is already the fill value, do nothing
        if original_value_id == fill_value_id:
            return

        # Use a queue for BFS (Breadth-First Search)
        q = collections.deque([(start_row, start_col)])
        visited = set([(start_row, start_col)])

        while q:
            r, c = q.popleft()

            # Change the value of the current data point to the specified fill_value_id
            self.data_matrix[r, c] = fill_value_id

            # Define neighbors (up, down, left, right) in matrix coordinates
            neighbors = [(r + 1, c), (r - 1, c), (r, c + 1), (r, c - 1)]

            for nr, nc in neighbors:
                # Check bounds and if the neighbor has the original value and hasn't been visited
                if (
                    0 <= nr < self.data_matrix.shape[0]
                    and 0 <= nc < self.data_matrix.shape[1]
                    and self.data_matrix[nr, nc] == original_value_id
                    and (nr, nc) not in visited
                ):
                    q.append((nr, nc))
                    visited.add((nr, nc))

        print(
            f"Contiguous region fill completed from ({start_x}, {start_y}) with value ID {fill_value_id}."
        )


def clarify_and_contrast_rgba(
    rgba_tuple: tuple[float, float, float, float],
    clarification_factor: float = 0.05,
    contrast_factor: float = 1.1,
) -> tuple[int, int, int, int]:
    """
    Clarifies (lightens) and enhances the contrast of an RGBA color tuple.

    Args:
        rgba_tuple (tuple[float, float, float, float]): A tuple representing the RGBA color, e.g., (r, g, b, a).
                                                        Each component (r, g, b) should be between 0 and 255.
                                                        The alpha (a) component should be between 0 and 255
                                                        (or 0 and 1, the function will handle it as 0-255 for consistency
                                                        and keep it unchanged).
        clarification_factor (float): A factor between 0.0 and 1.0 to control clarification.
                                      Higher values mean more clarification (closer to white).
                                      Default is 0.05.
        contrast_factor (float): A factor to control contrast. Values > 1.0 increase contrast,
                                 values < 1.0 decrease contrast. Default is 1.1.

    Returns:
        tuple[int, int, int, int]: A new tuple (r, g, b, a) with the clarified and contrasted color components,
                                   clamped to integer values between 0 and 255.
    """
    if not (isinstance(rgba_tuple, tuple) and len(rgba_tuple) == 4):
        raise ValueError(
            "Input rgba_tuple must be a tuple of 4 elements: (r, g, b, a)."
        )

    r, g, b, a = rgba_tuple

    # Ensure initial R, G, B values are within 0-255
    r = max(0, min(255, r))
    g = max(0, min(255, g))
    b = max(0, min(255, b))
    # Alpha can be 0-1 or 0-255, we'll just pass it through as is.
    # If it's expected to be 0-1 and needs to be converted, add that logic here.
    # For now, we assume it's consistent with R,G,B or handled externally.
    a = (
        max(0, min(255, a)) if a > 1 else max(0, min(1, a))
    )  # Clamp alpha if it's 0-1 or 0-255

    # Apply clarification (lightening)
    # Blends the color towards white (255, 255, 255)
    clarified_r = r + (255 - r) * clarification_factor
    clarified_g = g + (255 - g) * clarification_factor
    clarified_b = b + (255 - b) * clarification_factor

    # Apply contrast enhancement
    # Stretches values away from the midpoint (127.5)
    midpoint = 127.5
    contrasted_r = midpoint + (clarified_r - midpoint) * contrast_factor
    contrasted_g = midpoint + (clarified_g - midpoint) * contrast_factor
    contrasted_b = midpoint + (clarified_b - midpoint) * contrast_factor

    # Clamp the final R, G, B values to the 0-255 range
    final_r = int(max(0, min(255, contrasted_r)))
    final_g = int(max(0, min(255, contrasted_g)))
    final_b = int(max(0, min(255, contrasted_b)))

    # The alpha channel is typically not affected by clarification or contrast,
    # so we return it as is, or clamped to 0-255 if it was originally 0-1.
    final_a = int(max(0, min(255, a))) if a > 1 else a

    return (final_r, final_g, final_b, final_a)
