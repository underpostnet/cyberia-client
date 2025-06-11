import numpy as np
import math
import collections
import random
from typing import Union  # Import Union for type hinting compatibility


class SyntheticDataGenerator:
    """
    Manages and generates synthetic 2D data matrices with associated color mappings.

    Attributes:
        data_matrix (np.array): The 2D NumPy array representing the synthetic data.
                                Each element is an integer 'value_id'.
        value_map (dict): A mapping from integer data 'value_id's to RGBA color tuples (0-255).
                          Used for display purposes.
        last_generated_value_id (int | None): Stores the value ID of the last data
                                              generation operation.
    """

    # Constants for gradient shadow generation
    NUM_GRADIENT_SHADES = 10
    # Offset to create unique value_ids for gradient shades.
    # Assumes that normal value_ids do not exceed 999.
    VALUE_ID_GRADIENT_OFFSET = 1000

    def __init__(
        self, initial_data_matrix: Union[list, np.ndarray], value_mapping: dict
    ):
        """
        Initializes the SyntheticDataGenerator.

        Args:
            initial_data_matrix (list or np.array): The starting synthetic data matrix.
            value_mapping (dict): A dictionary mapping integer data values to RGBA tuples.
        """
        self.data_matrix = np.array(initial_data_matrix)
        self.value_map = value_mapping
        self.last_generated_value_id = None
        self._clipboard_data: Union[np.ndarray, None] = None  # To store the cut data

    def _convert_rgba_to_display_format(
        self, r: int, g: int, b: int, a: int
    ) -> tuple[float, float, float, float]:
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

    def get_display_color(self, data_value: int) -> tuple[float, float, float, float]:
        """
        Retrieves the Matplotlib-compatible RGBA color for a given data value ID.

        Args:
            data_value (int): The integer ID representing the data in the matrix.

        Returns:
            tuple: A tuple (r, g, b, a) with color values in the 0-1 range.
        """
        rgba_255 = self.value_map.get(data_value, (0, 0, 0, 0))
        return self._convert_rgba_to_display_format(*rgba_255)

    def _map_coordinates_to_matrix_indices(self, x: int, y: int) -> tuple[int, int]:
        """
        Converts (x, y) coordinates (where (0,0) is bottom-left) to
        (row_idx, col_idx) for the internal data matrix (where (0,0) is top-left).
        Clamps coordinates to be within matrix bounds.

        Args:
            x (int): The x-coordinate.
            y (int): The y-coordinate.

        Returns:
            tuple[int, int]: The corresponding (row_idx, col_idx) in the matrix.
        """
        matrix_height, matrix_width = self.data_matrix.shape

        row_idx = matrix_height - 1 - y
        col_idx = x

        clamped_row_idx = max(0, min(row_idx, matrix_height - 1))
        clamped_col_idx = max(0, min(col_idx, matrix_width - 1))
        return clamped_row_idx, clamped_col_idx

    def set_data_point(self, x: int, y: int, value_id: int):
        """
        Sets a single data point on the matrix with the specified value ID.
        (0,0) is considered the bottom-left of the data matrix.

        Args:
            x (int): The x-coordinate (column) of the data point.
            y (int): The y-coordinate (row) of the data point.
            value_id (int): The integer ID of the value to set.
        """
        row_idx, col_idx = self._map_coordinates_to_matrix_indices(x, y)
        # Ensure coordinates are within bounds before setting
        if (
            0 <= row_idx < self.data_matrix.shape[0]
            and 0 <= col_idx < self.data_matrix.shape[1]
        ):
            self.data_matrix[row_idx, col_idx] = value_id
            self.last_generated_value_id = value_id
        else:
            # Optionally, log a warning or raise an error if out of bounds
            # print(f"Warning: Attempted to set data point at ({x}, {y}) which is out of bounds.")
            pass

    def generate_rectangular_region(
        self, start_x: int, start_y: int, width: int, height: int, value_id: int
    ):
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
        end_x = start_x + width
        end_y = start_y + height

        for x in range(start_x, end_x):
            for y in range(start_y, end_y):
                self.set_data_point(x, y, value_id)

    def generate_parametric_curve_data(
        self,
        x_func: callable,
        y_func: callable,
        t_start: float,
        t_end: float,
        num_points: int,
        value_id: int,
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
            x_float = x_func(t)
            y_float = y_func(t)

            x_int = int(round(x_float))
            y_int = int(round(y_float))

            self.set_data_point(x_int, y_int, value_id)

    def get_coordinates_in_region(
        self, x1: int, y1: int, x2: int, y2: int
    ) -> list[tuple[int, int]]:
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
        min_x = min(x1, x2)
        max_x = max(x1, x2)
        min_y = min(y1, y2)
        max_y = max(y1, y2)

        coordinates = []
        matrix_height, matrix_width = self.data_matrix.shape

        for y in range(min_y, max_y + 1):
            for x in range(min_x, max_x + 1):
                clamped_x = max(0, min(x, matrix_width - 1))
                clamped_y = max(0, min(y, matrix_height - 1))
                coordinates.append((clamped_x, clamped_y))

        # Use a set to remove duplicates and then convert back to list
        return list(set(coordinates))

    def _apply_gradient_shadow(
        self,
        filled_coordinates: list[tuple[int, int]],
        base_value_id: int,
        intensity_factor: float,
        direction: str,
    ):
        """
        Applies a gradient shadow to the specified coordinates within the filled region.
        This modifies the `data_matrix` by assigning new `value_id`s for gradient shades
        and updating the `value_map` accordingly.

        Args:
            filled_coordinates (list[tuple[int, int]]): List of (row, col) matrix indices
                                                         of the filled region.
            base_value_id (int): The original value ID used for the fill.
            intensity_factor (float): Factor (0.0 to 1.0) controlling shadow intensity.
            direction (str): Direction of the gradient ("left_to_right", "right_to_left",
                             "top_to_bottom", "bottom_to_top").
        """
        if not filled_coordinates:
            return

        # Convert (x,y) coordinates to (row,col) matrix indices for internal use
        # and then back to (x,y) for min/max calculations to get the bounding box
        # of the *filled region* in the original coordinate system.
        # This is important for consistent gradient direction.
        x_coords_filled = [col for _, col in filled_coordinates]
        y_coords_filled = [
            self.data_matrix.shape[0] - 1 - row for row, _ in filled_coordinates
        ]

        min_x_filled = min(x_coords_filled)
        max_x_filled = max(x_coords_filled)
        min_y_filled = min(y_coords_filled)
        max_y_filled = max(y_coords_filled)

        base_rgba_255 = self.value_map.get(base_value_id, (0, 0, 0, 255))
        base_r, base_g, base_b, base_a = base_rgba_255

        for row_idx, col_idx in filled_coordinates:
            # Convert matrix indices back to (x,y) for gradient calculation
            x = col_idx
            y = self.data_matrix.shape[0] - 1 - row_idx

            distance = 0
            max_distance = 0

            if direction == "left_to_right":
                distance = x - min_x_filled
                max_distance = max_x_filled - min_x_filled
            elif direction == "right_to_left":
                distance = max_x_filled - x
                max_distance = max_x_filled - min_x_filled
            elif direction == "top_to_bottom":
                distance = y - min_y_filled
                max_distance = max_y_filled - min_y_filled
            elif direction == "bottom_to_top":
                distance = max_y_filled - y
                max_distance = max_y_filled - min_y_filled

            # Calculate shadow factor (0.0 to 1.0)
            shadow_factor = distance / max_distance if max_distance != 0 else 0

            # Map shadow factor to a shade index
            shade_index = int(shadow_factor * (self.NUM_GRADIENT_SHADES - 1))
            shade_index = max(0, min(shade_index, self.NUM_GRADIENT_SHADES - 1))

            # Calculate darkening amount
            # The darkening should be more pronounced for higher shade_index
            current_darken_amount = (
                (shade_index / (self.NUM_GRADIENT_SHADES - 1)) * intensity_factor * 255
            )
            current_darken_amount = min(current_darken_amount, 255)

            # Apply darkening to RGB components
            shaded_r = max(0, int(base_r - current_darken_amount))
            shaded_g = max(0, int(base_g - current_darken_amount))
            shaded_b = max(0, int(base_b - current_darken_amount))

            # Generate a unique value ID for this specific shade
            # Ensure the generated ID does not clash with existing base IDs
            gradient_value_id = (
                (base_value_id * self.VALUE_ID_GRADIENT_OFFSET) + shade_index + 1
            )

            # Add the new gradient color to the value_map if it doesn't exist
            if gradient_value_id not in self.value_map:
                self.value_map[gradient_value_id] = (
                    shaded_r,
                    shaded_g,
                    shaded_b,
                    base_a,
                )

            # Set the data matrix point to the new gradient value ID
            self.data_matrix[row_idx, col_idx] = gradient_value_id

    def contiguous_region_fill(
        self,
        start_x: int,
        start_y: int,
        fill_value_id: int = 0,
        gradient_shadow: bool = False,  # Renamed from apply_gradient_shadow
        intensity_factor: float = 0.5,
        direction: str = None,  # Renamed from gradient_direction
    ):
        """
        Performs a contiguous region fill operation starting from a given (x, y) coordinate
        (where (0,0) is bottom-left).
        Changes the value of adjacent data points to 'fill_value_id'
        if their original value matches the starting data point's value, until a different
        value is found. Optionally applies a gradient shadow.

        Args:
            start_x (int): The starting x-coordinate for the fill.
            start_y (int): The starting y-coordinate for the fill.
            fill_value_id (int): The value ID to fill with. Defaults to 0 (white).
            gradient_shadow (bool): If True, a gradient shadow will be applied over the filled area.
            intensity_factor (float): A factor (0.0 to 1.0) controlling the intensity of the shadow.
                                      0.0 means no shadow, 1.0 means maximum darkening.
            direction (str, optional): The direction of the gradient shadow.
                                       Can be "left_to_right", "right_to_left", "top_to_bottom",
                                       "bottom_to_top". If None, a random direction is chosen.
        """
        start_row, start_col = self._map_coordinates_to_matrix_indices(start_x, start_y)

        if not (
            0 <= start_row < self.data_matrix.shape[0]
            and 0 <= start_col < self.data_matrix.shape[1]
        ):
            print(
                f"Error: Start coordinates ({start_x}, {start_y}) are out of bounds for region fill."
            )
            return

        original_value_id = self.data_matrix[start_row, start_col]

        # If the original value is already the fill value and no gradient is requested, do nothing.
        if original_value_id == fill_value_id and not gradient_shadow:
            return

        q = collections.deque([(start_row, start_col)])
        visited = set([(start_row, start_col)])
        filled_coordinates = []

        while q:
            r, c = q.popleft()

            # Add to filled_coordinates regardless of gradient_shadow, as we need this list
            # for both direct fill and gradient application.
            filled_coordinates.append((r, c))

            neighbors = [(r + 1, c), (r - 1, c), (r, c + 1), (r, c - 1)]

            for nr, nc in neighbors:
                if (
                    0 <= nr < self.data_matrix.shape[0]
                    and 0 <= nc < self.data_matrix.shape[1]
                    and self.data_matrix[nr, nc] == original_value_id
                    and (nr, nc) not in visited
                ):
                    q.append((nr, nc))
                    visited.add((nr, nc))

        # First, fill all identified coordinates with the base fill_value_id
        # This ensures a consistent base color before applying any gradient.
        for r, c in filled_coordinates:
            self.data_matrix[r, c] = fill_value_id

        if gradient_shadow:
            if direction is None:
                directions = [
                    "left_to_right",
                    "right_to_left",
                    "top_to_bottom",
                    "bottom_to_top",
                ]
                selected_direction = random.choice(directions)
            else:
                selected_direction = direction

            self._apply_gradient_shadow(
                filled_coordinates, fill_value_id, intensity_factor, selected_direction
            )

    def cut_region(
        self, x1: int, y1: int, x2: int, y2: int, clear_value: Union[int, None] = None
    ):
        """
        Cuts a rectangular region defined by (x1, y1) and (x2, y2) user coordinates
        (where (0,0) is bottom-left for user coordinates) and stores it in the internal clipboard.
        The clipboard's (0,0) index will correspond to the top-left point (min_x_user, max_y_user)
        of the cut region.
        Optionally clears the cut region in the main data matrix.

        Args:
            x1 (int): X-coordinate of the first corner.
            y1 (int): Y-coordinate of the first corner.
            x2 (int): X-coordinate of the second corner.
            y2 (int): Y-coordinate of the second corner.
            clear_value (Union[int, None], optional): Value to fill the cut region with.
                                                     If None, the region is not cleared. Defaults to None.
        """
        min_x_user = min(x1, x2)
        max_x_user = max(x1, x2)
        min_y_user = min(y1, y2)
        max_y_user = max(y1, y2)

        region_width = max_x_user - min_x_user + 1
        region_height = max_y_user - min_y_user + 1

        if region_width <= 0 or region_height <= 0:
            self._clipboard_data = None
            # print("Warning: Cut region has zero or negative dimension.")
            return

        self._clipboard_data = np.zeros((region_height, region_width), dtype=int)

        for r_idx_clip in range(region_height):
            for c_idx_clip in range(region_width):
                current_x_user = min_x_user + c_idx_clip
                # Clipboard row 0 (r_idx_clip=0) corresponds to max_y_user (top row of selection)
                current_y_user = max_y_user - r_idx_clip

                matrix_r, matrix_c = self._map_coordinates_to_matrix_indices(
                    current_x_user, current_y_user
                )

                # Read from data_matrix (clamped by _map_coordinates_to_matrix_indices)
                self._clipboard_data[r_idx_clip, c_idx_clip] = self.data_matrix[
                    matrix_r, matrix_c
                ]

                # Clear original spot if requested
                if clear_value is not None:
                    self.set_data_point(current_x_user, current_y_user, clear_value)
        # print(f"Cut region of shape {self._clipboard_data.shape} to clipboard.")

    def paste_region(self, paste_x_start_user: int, paste_y_start_user: int):
        """
        Pastes the data from the internal clipboard to the data matrix,
        starting at (paste_x_start_user, paste_y_start_user) as the top-left
        corner of the pasted region.

        Args:
            paste_x_start_user (int): The x-coordinate for the top-left of the paste.
            paste_y_start_user (int): The y-coordinate for the top-left of the paste.
        """
        if self._clipboard_data is None:
            # print("Warning: Clipboard is empty. Nothing to paste.")
            return

        clip_height, clip_width = self._clipboard_data.shape

        for r_clip in range(clip_height):
            for c_clip in range(clip_width):
                value_to_paste = self._clipboard_data[r_clip, c_clip]

                # Target user coordinates
                target_x_user = paste_x_start_user + c_clip
                # r_clip=0 is the top row of clipboard, should be pasted at paste_y_start_user
                target_y_user = paste_y_start_user - r_clip

                self.set_data_point(target_x_user, target_y_user, value_to_paste)
        # print(f"Pasted region at ({paste_x_start_user}, {paste_y_start_user}).")


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
        contrast_factor (float): A factor to control contrast. Values > 1.0 increase contrast,
                                 values < 1.0 decrease contrast.

    Returns:
        tuple[int, int, int, int]: A new tuple (r, g, b, a) with the clarified and contrasted color components,
                                   clamped to integer values between 0 and 255.
    """
    if not (isinstance(rgba_tuple, tuple) and len(rgba_tuple) == 4):
        raise ValueError(
            "Input rgba_tuple must be a tuple of 4 elements: (r, g, b, a)."
        )

    r, g, b, a = rgba_tuple

    r = max(0, min(255, r))
    g = max(0, min(255, g))
    b = max(0, min(255, b))
    a = max(0, min(255, a)) if a > 1 else max(0, min(1, a))

    clarified_r = r + (255 - r) * clarification_factor
    clarified_g = g + (255 - g) * clarification_factor
    clarified_b = b + (255 - b) * clarification_factor

    midpoint = 127.5
    contrasted_r = midpoint + (clarified_r - midpoint) * contrast_factor
    contrasted_g = midpoint + (clarified_g - midpoint) * contrast_factor
    contrasted_b = midpoint + (clarified_b - midpoint) * contrast_factor

    final_r = int(max(0, min(255, contrasted_r)))
    final_g = int(max(0, min(255, contrasted_g)))
    final_b = int(max(0, min(255, contrasted_b)))

    final_a = int(max(0, min(255, a))) if a > 1 else a

    return (final_r, final_g, final_b, final_a)
