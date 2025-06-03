import numpy as np
import math
import random
import matplotlib.pyplot as plt  # Import matplotlib for rendering

# This file provides an importable tool API to work with synthetic data,
# including focus move logic and various generation patterns.

# Forward declaration for type hinting to avoid circular imports
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from object_layer.object_layer_sdg import SyntheticDataGenerator


class SyntheticDataToolAPI:
    """
    A tool API for generating complex synthetic data patterns and applying
    "focus move" logic on a SyntheticDataGenerator instance.
    """

    def __init__(self, data_generator: "SyntheticDataGenerator"):
        """
        Initializes the SyntheticDataToolAPI with a SyntheticDataGenerator instance.

        Args:
            data_generator (SyntheticDataGenerator): An instance of the SyntheticDataGenerator.
        """
        self.data_generator = data_generator
        self.data_matrix_height, self.data_matrix_width = (
            self.data_generator.data_matrix.shape
        )

    def create_empty_canvas(self, size: int, fill_value: int = 0):
        """
        Creates a new empty NxN canvas matrix and sets it as the data_matrix
        for the SyntheticDataGenerator.

        Args:
            size (int): The dimension (N) of the square canvas (NxN).
            fill_value (int): The integer value to fill the canvas with. Defaults to 0 (white).
        """
        if size <= 0:
            raise ValueError("Canvas size must be a positive integer.")
        new_matrix = np.full((size, size), fill_value, dtype=int)
        self.data_generator.data_matrix = new_matrix
        self.data_matrix_height, self.data_matrix_width = new_matrix.shape

    def draw_circle(self, center_x: int, center_y: int, radius: int, value_id: int):
        """
        Draws a filled circle on the data matrix.

        Args:
            center_x (int): The x-coordinate of the circle's center.
            center_y (int): The y-coordinate of the circle's center.
            radius (int): The radius of the circle.
            value_id (int): The integer ID of the value to fill the circle with.
        """
        for y_offset in range(-radius, radius + 1):
            for x_offset in range(-radius, radius + 1):
                if x_offset**2 + y_offset**2 <= radius**2:
                    x = center_x + x_offset
                    y = center_y + y_offset
                    # set_data_point handles bounds checking internally
                    self.data_generator.set_data_point(x, y, value_id)

    def generate_pattern_from_coordinates(
        self,
        initial_x_pos: int,
        initial_y_pos: int,
        value_id: int,
        coordinates_list: list[list[int]],
        filter_func=None,
    ):
        """
        Generates a pattern on the data matrix by drawing data points based on a list of
        relative coordinates, starting from an initial position.

        Args:
            initial_x_pos (int): The starting x-coordinate for the pattern.
            initial_y_pos (int): The starting y-coordinate for the pattern.
            value_id (int): The value ID to use for the data points.
            coordinates_list (list): A list of (dx, dy) tuples representing relative movements.
            filter_func (callable, optional): A function to apply to each coordinate (x, y)
                                              before drawing. Defaults to None.
        """
        pointer = [initial_x_pos, initial_y_pos]
        for dx, dy in coordinates_list:
            if filter_func:
                dx, dy = filter_func(dx, dy)
            self.data_generator.set_data_point(  # Changed from generate_data_point to set_data_point
                pointer[0] + dx, pointer[1] + dy, value_id
            )
            pointer = [pointer[0] + dx, pointer[1] + dy]

    def create_coordinate_pattern(
        self, pattern_type: str = "hair-lock"
    ) -> list[list[int]]:
        """
        Creates a predefined list of relative coordinates for specific patterns.

        Args:
            pattern_type (str): The type of pattern to create (e.g., "hair-lock").

        Returns:
            list: A list of (dx, dy) tuples representing relative coordinates.
        """
        coordinates = []
        if pattern_type == "hair-lock":
            coordinates = [
                [0, 0],
                [-1, 1],
                [-1, 0],
                [-1, 0],
                [-1, -1],
                [-1, -1],
                [-1, -1],
                [0, -1],
            ]
            length_factor = random.randint(0, 5)
            for _ in range(length_factor):
                coordinates.append([0, -1])
        # Add more pattern types here as needed
        return coordinates

    def apply_default_skin_template_fill(self, display_color_palette: dict):
        """
        Applies a default 'skin' template fill based on common data points.
        This is a specific generation logic for a particular data structure.

        Args:
            display_color_palette (dict): The color palette used for display,
                                          to pick random colors for filling.
        """
        # Note: This method uses hardcoded coordinates specific to the default template.
        # For a truly generic SDG, these coordinates would be passed as parameters
        # or derived from a more abstract template definition.

        # Fill main body area
        skin_color_id = random.choice(list(range(9, 14)))
        self.data_generator.contiguous_region_fill(12, 12, fill_value_id=skin_color_id)
        self.data_generator.contiguous_region_fill(7, 4, fill_value_id=skin_color_id)
        self.data_generator.contiguous_region_fill(18, 4, fill_value_id=skin_color_id)

        # Fill specific internal regions with random colors
        self.data_generator.contiguous_region_fill(
            13,
            7,
            fill_value_id=random.choice(list(range(2, 9))),
            gradient_shadow=True,
            intensity_factor=0.5,
            direction="bottom_to_top",
        )
        self.data_generator.contiguous_region_fill(
            12, 4, fill_value_id=random.choice(list(range(2, 9)))
        )

        # Fill shoes area
        shoes_color_id = random.choice(list(range(2, 9)))
        self.data_generator.contiguous_region_fill(9, 2, fill_value_id=shoes_color_id)
        self.data_generator.contiguous_region_fill(15, 2, fill_value_id=shoes_color_id)

    def generate_complex_parametric_curve(
        self, curve_type: str, initial_x_pos: float, initial_y_pos: float, value_id: int
    ):
        """
        Generates a parametric curve on the data matrix with random parameters.

        Args:
            curve_type (str): The type of parametric curve to generate.
            initial_x_pos (float): The initial x-position for the curve's origin/center.
            initial_y_pos (float): The initial y-position for the curve's origin/center.
            value_id (int): The value ID to use for the data points on the curve.
        """
        num_points = random.randint(50, 50)  # Fixed number of points for consistency

        x_func, y_func, t_start, t_end = None, None, None, None

        if curve_type == "parabola":
            parabola_center_x = initial_x_pos
            parabola_base_y = initial_y_pos
            scale_x = random.uniform(5, 15)
            scale_y = random.uniform(5, 15)
            a = random.uniform(-0.5, 0.5)
            x_func, y_func = self._get_parabola_funcs(
                parabola_center_x, parabola_base_y, scale_x, scale_y, a
            )
            t_start = -1.0
            t_end = 1.0

        elif curve_type == "sigmoid":
            sigmoid_center_x = initial_x_pos
            sigmoid_midpoint_y = initial_y_pos
            scale_x = random.uniform(5, 15)
            scale_y = random.uniform(5, 15)
            k = random.uniform(0.5, 5.0)
            x0 = random.uniform(-0.5, 0.5)
            x_func, y_func = self._get_sigmoid_funcs(
                sigmoid_center_x, sigmoid_midpoint_y, scale_x, scale_y, k, x0
            )
            t_start = -5.0
            t_end = 5.0

        elif curve_type == "sine":
            sine_start_x = initial_x_pos
            sine_y_axis_offset = initial_y_pos
            amplitude = random.uniform(3, 10)
            frequency = random.uniform(0.5, 3.0)
            x_func, y_func = self._get_sine_funcs(
                sine_start_x, sine_y_axis_offset, amplitude, frequency
            )
            t_start = 0
            t_end = random.uniform(self.data_matrix_width / 2, self.data_matrix_width)

        elif curve_type == "linear":
            line_start_x = initial_x_pos
            line_start_y = initial_y_pos
            line_end_x = random.uniform(0, self.data_matrix_width)
            line_end_y = random.uniform(0, self.data_matrix_height)
            x_func, y_func = self._get_linear_funcs(
                line_start_x, line_start_y, line_end_x, line_end_y
            )
            t_start = 0.0
            t_end = 1.0

        elif curve_type == "cubic":
            cubic_center_x = initial_x_pos
            cubic_center_y = initial_y_pos
            scale_x = random.uniform(5, 15)
            scale_y = random.uniform(5, 15)
            a = random.uniform(-0.1, 0.1)
            b = random.uniform(-0.5, 0.5)
            c = random.uniform(-0.5, 0.5)
            x_func, y_func = self._get_cubic_funcs(
                cubic_center_x, cubic_center_y, scale_x, scale_y, a, b, c
            )
            t_start = -1.0
            t_end = 1.0

        elif curve_type == "circle_arc":
            arc_center_x = initial_x_pos
            arc_center_y = initial_y_pos
            radius = random.uniform(
                5, min(self.data_matrix_width, self.data_matrix_height) / 2 - 2
            )
            start_angle = random.uniform(0, 2 * math.pi)
            end_angle = start_angle + random.uniform(math.pi / 4, 1.5 * math.pi)
            x_func, y_func = self._get_circle_arc_funcs(
                arc_center_x, arc_center_y, radius, start_angle, end_angle
            )
            t_start = start_angle
            t_end = end_angle

        elif curve_type == "spiral":
            spiral_center_x = initial_x_pos
            spiral_center_y = initial_y_pos
            radius_growth_rate = random.uniform(0.5, 2.0)
            angular_speed = random.uniform(1.0, 3.0)
            x_func, y_func = self._get_spiral_funcs(
                spiral_center_x, spiral_center_y, radius_growth_rate, angular_speed
            )
            t_start = 0
            t_end = random.uniform(2 * math.pi, 6 * math.pi)

        if x_func and y_func:
            self.data_generator.generate_parametric_curve_data(
                x_func, y_func, t_start, t_end, num_points, value_id
            )

    def render_data_matrix_to_subplot(self, ax: plt.Axes, subplot_index: int):
        """
        Renders the synthetic data matrix onto a Matplotlib subplot.

        Args:
            ax (matplotlib.axes.Axes): The Matplotlib axes object to draw on.
            subplot_index (int): The index of the current subplot for titling.
        """
        # Update matrix dimensions in case they changed (e.g., with create_empty_canvas)
        self.data_matrix_height, self.data_matrix_width = (
            self.data_generator.data_matrix.shape
        )

        # Set subplot limits and labels
        ax.set_xlim(0, self.data_matrix_width)
        ax.set_ylim(0, self.data_matrix_height)
        ax.set_xlabel("Data X-coordinate")
        ax.set_ylabel("Data Y-coordinate")
        ax.set_aspect("equal", adjustable="box")
        ax.set_title(
            f"Synthetic Data {subplot_index+1}"
        )  # Add a title for each subplot

        # Iterate over the matrix to draw each data point as a colored square
        # The matrix now stores data as if (0,0) is top-left, but we plot it
        # as if (0,0) is bottom-left. So, the plotting logic needs to invert Y.
        for row_idx in range(self.data_matrix_height):
            for col_idx in range(self.data_matrix_width):
                data_value = self.data_generator.data_matrix[row_idx, col_idx]
                color = self.data_generator.get_display_color(data_value)

                # Calculate the coordinates for the current data point's square
                # Invert y-coordinate for drawing to match matrix (0 at top) to plot (0 at bottom)
                # This is the crucial part that aligns the matrix data with Matplotlib's y-axis.
                square_y_bottom = self.data_matrix_height - 1 - row_idx
                square_x_left = col_idx

                x_coords = [
                    square_x_left,
                    square_x_left + 1,
                    square_x_left + 1,
                    square_x_left,
                ]
                y_coords = [
                    square_y_bottom,
                    square_y_bottom,
                    square_y_bottom + 1,
                    square_y_bottom + 1,
                ]

                ax.fill(x_coords, y_coords, color=color, edgecolor="none")

        # Draw horizontal grid lines
        for y in range(self.data_matrix_height + 1):
            ax.axhline(y, color="lightgray", linestyle="-", linewidth=0.5)

        # Draw vertical grid lines
        for x in range(self.data_matrix_width + 1):
            ax.axvline(x, color="lightgray", linestyle="-", linewidth=0.5)

    # --- Parametric Curve Functions (Helper Methods) ---
    # These functions return x and y functions for a given curve type,
    # allowing for flexible positioning and scaling.
    # They return (x, y) coordinates where (0,0) is bottom-left.

    def _get_parabola_funcs(
        self, x_center: float, y_base: float, scale_x: float, scale_y: float, a: float
    ):
        """Returns x and y functions for a parabola."""
        x_func = lambda t: x_center + scale_x * t
        y_func = lambda t: y_base + scale_y * a * (t**2)
        return x_func, y_func

    def _get_sigmoid_funcs(
        self,
        x_center: float,
        y_midpoint: float,
        scale_x: float,
        scale_y: float,
        k: float,
        x0: float,
    ):
        """Returns x and y functions for a sigmoid curve."""
        x_func = lambda t: x_center + scale_x * t
        y_func = lambda t: y_midpoint + scale_y * (1 / (1 + math.exp(-k * (t - x0))))
        return x_func, y_func

    def _get_sine_funcs(
        self, x_start: float, y_axis_offset: float, amplitude: float, frequency: float
    ):
        """Returns x and y functions for a sine wave."""
        x_func = lambda t: x_start + t
        y_func = lambda t: y_axis_offset + amplitude * math.sin(frequency * t)
        return x_func, y_func

    def _get_linear_funcs(
        self, x_start: float, y_start: float, x_end: float, y_end: float
    ):
        """Returns x and y functions for a linear curve (straight line)."""
        x_func = lambda t: x_start + t * (x_end - x_start)
        y_func = lambda t: y_start + t * (y_end - y_start)
        return x_func, y_func

    def _get_cubic_funcs(
        self,
        x_center: float,
        y_center: float,
        scale_x: float,
        scale_y: float,
        a: float,
        b: float,
        c: float,
    ):
        """Returns x and y functions for a cubic curve."""
        x_func = lambda t: x_center + scale_x * t
        y_func = lambda t: y_center + scale_y * (a * (t**3) + b * (t**2) + c * t)
        return x_func, y_func

    def _get_circle_arc_funcs(
        self,
        center_x: float,
        center_y: float,
        radius: float,
        start_angle: float,
        end_angle: float,
    ):
        """Returns x and y functions for a circular arc."""
        x_func = lambda t: center_x + radius * math.cos(t)
        y_func = lambda t: center_y + radius * math.sin(t)
        return x_func, y_func

    def _get_spiral_funcs(
        self,
        center_x: float,
        center_y: float,
        radius_growth_rate: float,
        angular_speed: float,
    ):
        """Returns x and y functions for a spiral curve."""
        x_func = lambda t: center_x + (radius_growth_rate * t) * math.cos(
            angular_speed * t
        )
        y_func = lambda t: center_y + (radius_growth_rate * t) * math.sin(
            angular_speed * t
        )
        return x_func, y_func
