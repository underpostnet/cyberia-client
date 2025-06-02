# Synthetic Data Generation (SDG) script/tool

import matplotlib.pyplot as plt
import numpy as np
import random
import math
import argparse
from tabulate import tabulate

# --- Configuration Constants ---
# Original pixel art matrix
BASE_MATRIX = [
    [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 1, 1, 0, 0, 1, 1, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
]

# Base color map for the pixel art (white and black)
BASE_COLORS = np.array(
    [[1.0, 1.0, 1.0, 1.0], [0.0, 0.0, 0.0, 1.0]]  # Index 0: White  # Index 1: Black
)

# Region for 'hair' seed positions
HAIR_SPAWN_ROWS = (0, 5)
HAIR_SPAWN_COLS = (2, 22)

# Five shades of common human skin tones (RGBA)
SKIN_TONE_COLORS = [
    [255 / 255, 224 / 255, 189 / 255, 225 / 255],  # Light
    [240 / 255, 192 / 255, 150 / 255, 225 / 255],  # Medium-Light
    [218 / 255, 166 / 255, 126 / 255, 225 / 255],  # Medium
    [186 / 255, 128 / 255, 92 / 255, 225 / 255],  # Medium-Dark
    [139 / 255, 69 / 255, 19 / 255, 225 / 255],  # Dark
]

# --- Parametric Curve Definitions ---
import math


class ParametricCurve:
    """Defines various parametric curves that can be used for strokes."""

    def __init__(self, curve_name, curve_method, codomain_type, domain_type):
        self.name = curve_name
        self.method = curve_method
        self.codomain_type = codomain_type
        self.domain_type = domain_type

    @staticmethod
    def _parabola_method(t_relative, scale_y):
        """Calculates y for a parabola, with its vertex at (0,0) in relative coordinates."""
        return scale_y * (t_relative**2)

    @staticmethod
    def _sigmoid_method(t_relative, scale_y):
        """Calculates y for a sigmoid, with its inflection point at (0,0) in relative coordinates."""
        scaled_t = t_relative / 5
        return scale_y * (1 / (1 + math.exp(-scaled_t)) - 0.5)

    @staticmethod
    def _sine_method(t_relative, scale_y):
        """Calculates y for a sine wave, with a zero-crossing at (0,0) in relative coordinates."""
        frequency = 0.5
        return scale_y * math.sin(frequency * t_relative)

    @staticmethod
    def _linear_method(t_relative, scale_y):
        """Calculates y for a linear (straight line) function passing through the origin."""
        return scale_y * t_relative

    @staticmethod
    def _cubic_method(t_relative, scale_y):
        """Calculates y for a cubic function centered at the origin."""
        return scale_y * (t_relative**3)

    @staticmethod
    def _circle_arc_method(t_relative, scale_y):
        """Calculates y for a circular arc of unit radius centered at (0,0)."""
        if abs(t_relative) > 1:
            return None  # Out of domain
        return scale_y * math.sqrt(1 - t_relative**2)


# Pre-defined parametric curve types with their codomain and domain types
PARAMETRIC_CURVE_TYPES = {
    "parabola": ParametricCurve(
        "parabola",
        ParametricCurve._parabola_method,
        "Unbounded (Positive)",
        "All Real Numbers",
    ),
    "sigmoid": ParametricCurve(
        "sigmoid",
        ParametricCurve._sigmoid_method,
        "Bounded (0-1 Scaled)",
        "All Real Numbers",
    ),
    "sine": ParametricCurve(
        "sine",
        ParametricCurve._sine_method,
        "Bounded (-1 to 1 Scaled)",
        "All Real Numbers",
    ),
    "linear": ParametricCurve(
        "linear",
        ParametricCurve._linear_method,
        "Unbounded (Both Directions)",
        "All Real Numbers",
    ),
    "cubic": ParametricCurve(
        "cubic",
        ParametricCurve._cubic_method,
        "Unbounded (Both Directions)",
        "All Real Numbers",
    ),
    "circle_arc": ParametricCurve(
        "circle_arc",
        ParametricCurve._circle_arc_method,
        "Bounded (Upper Semicircle)",
        "[-1, 1]",
    ),
}


# --- Seed Position Abstractions ---
class SeedPosition:
    """Abstract base class for different types of seed positions."""

    def __init__(self, row, col, marker_char, marker_color, bbox_facecolor):
        self.row = row
        self.col = col
        self.marker_char = marker_char
        self.marker_color = marker_color
        self.bbox_facecolor = bbox_facecolor

    def get_coordinates(self):
        """Returns the (row, col) coordinates of the seed position."""
        return (self.row, self.col)

    def get_marker_properties(self):
        """Returns properties for rendering the seed position marker on the plot."""
        return {
            "s": self.marker_char,
            "color": self.marker_color,
            "bbox": dict(
                facecolor=self.bbox_facecolor,
                alpha=0.7,
                edgecolor="none",
                boxstyle="round,pad=0.2",
            ),
        }

    def can_generate_stroke(self):
        """Determines if this seed type should generate a parametric curve stroke."""
        raise NotImplementedError("Subclasses must implement this method unarmed.")


class HairSeed(SeedPosition):
    """Represents a 'hair' type seed position, generating a parametric curve stroke."""

    def __init__(self, row, col):
        super().__init__(row, col, "H", "black", "none")
        self.type = "hair"

    def can_generate_stroke(self):
        return True


class FillSeed(SeedPosition):
    """Represents a 'fill' type seed position, which does not generate a parametric curve stroke."""

    def __init__(self, row, col):
        super().__init__(row, col, "F", "black", "none")
        self.type = "fill"

    def can_generate_stroke(self):
        return False


class FillGradientShadowSeed(SeedPosition):
    """Represents a 'fill-gradient-shadow' type seed position."""

    def __init__(self, row, col):
        super().__init__(row, col, "G", "black", "none")
        self.type = "fill-gradient-shadow"

    def can_generate_stroke(self):
        return False


# --- Pixel Art Generation Logic ---
class PixelArtGenerator:
    """Generates and renders pixel art with dynamic elements and provides summaries."""

    def __init__(
        self,
        base_matrix_data,
        hair_spawn_rows,
        hair_spawn_cols,
        parametric_curve_types,
        skin_tone_colors,
    ):
        self.base_matrix = np.array(base_matrix_data)
        self.matrix_rows, self.matrix_cols = self.base_matrix.shape
        self.hair_spawn_rows = hair_spawn_rows
        self.hair_spawn_cols = hair_spawn_cols
        self.parametric_curve_types = parametric_curve_types
        self.skin_tone_colors = skin_tone_colors
        self.all_curve_instance_summaries = []
        self.graph_seed_counts_summaries = []
        self._curve_instance_id_counter = 0

    def _generate_random_rgba_color(self):
        """Generates a random RGBA color."""
        return [random.random(), random.random(), random.random(), 1.0]

    def _get_random_hair_seeds(self, min_count, max_count):
        """Generates random 'hair' seed positions within the defined spawn area."""
        num_seeds = random.randint(min_count, max_count)
        possible_coords = [
            (r, c)
            for r in range(self.hair_spawn_rows[0], self.hair_spawn_rows[1] + 1)
            for c in range(self.hair_spawn_cols[0], self.hair_spawn_cols[1] + 1)
        ]
        num_seeds = min(num_seeds, len(possible_coords))
        selected_coords = random.sample(possible_coords, num_seeds)
        return [HairSeed(r, c) for r, c in selected_coords]

    def _get_fill_seeds(self, specific_coords_xy, existing_hair_coords, mode=None):
        """
        Generates 'fill' seed positions based on specific (x,y) coordinates provided.
        If specific_coords_xy is empty, it defaults to a single fill seed at the center.
        In 'skin-default-1' mode, adds 1 random fill-gradient-shadow seed and a default fill seed.
        In 'skin-default-2' mode, adds 1 random fill-gradient-shadow seed and a default fill seed.
        """
        fill_seeds = []

        # Add explicitly defined fill seeds (these take precedence)
        if specific_coords_xy:
            for x, y in specific_coords_xy:
                if 0 <= y < self.matrix_rows and 0 <= x < self.matrix_cols:
                    fill_seeds.append(FillSeed(y, x))
                else:
                    print(
                        f"Warning: Explicit fill seed coordinate ({x},{y}) is out of matrix bounds or already occupied and will be skipped."
                    )

        # Handle mode-specific fill seeds
        if mode in ["skin-default-1", "skin-default-2"]:
            # Add 1 random fill-gradient-shadow seed
            num_gradient_seeds = 1
            possible_coords_for_gradient = [
                (r, c)
                for r in range(self.hair_spawn_rows[0], self.hair_spawn_rows[1] + 1)
                for c in range(self.hair_spawn_cols[0], self.hair_spawn_cols[1] + 1)
            ]

            # Collect all coordinates already taken (hair seeds + explicit fill seeds)
            all_taken_coords = existing_hair_coords.union(
                {seed.get_coordinates() for seed in fill_seeds}
            )
            available_coords_for_gradient = [
                coord
                for coord in possible_coords_for_gradient
                if coord not in all_taken_coords
            ]

            if available_coords_for_gradient:  # Ensure there are available spots
                selected_gradient_coords = random.sample(
                    available_coords_for_gradient,
                    min(num_gradient_seeds, len(available_coords_for_gradient)),
                )
                for r, c in selected_gradient_coords:
                    fill_seeds.append(FillGradientShadowSeed(r, c))
                    all_taken_coords.add(
                        (r, c)
                    )  # Update taken coords for the next check

            # If no specific_coords_xy were provided, add a default FillSeed at the center
            if not specific_coords_xy:
                default_fill_coord = (self.matrix_rows // 2, self.matrix_cols // 2)
                if default_fill_coord not in all_taken_coords:
                    fill_seeds.append(
                        FillSeed(default_fill_coord[0], default_fill_coord[1])
                    )
                else:
                    print(
                        f"Warning: Default fill seed position {default_fill_coord} is already occupied in {mode} mode."
                    )

        # Default behavior if no specific fill seeds and not in a special mode
        elif (
            not specific_coords_xy
        ):  # This covers mode=None, skin-default, skin-default-0
            if not fill_seeds:  # Only add if no other seeds were added yet
                fill_seeds.append(
                    FillSeed(self.matrix_rows // 2, self.matrix_cols // 2)
                )

        return fill_seeds

    def _flood_fill_get_coords(
        self, matrix_to_check, start_row, start_col, target_color_value
    ):
        """
        Performs a flood fill operation and returns the coordinates of all filled pixels,
        without modifying the matrix. It operates on a copy or the original matrix
        to determine the fillable area based on a target color value.
        """
        rows, cols = matrix_to_check.shape
        filled_coords = []

        if not (0 <= start_row < rows and 0 <= start_col < cols):
            return filled_coords

        if matrix_to_check[start_row, start_col] != target_color_value:
            return filled_coords

        q = [(start_row, start_col)]
        visited = set()
        visited.add((start_row, start_col))
        filled_coords.append((start_row, start_col))

        while q:
            r, c = q.pop(0)

            neighbors = [(r + 1, c), (r - 1, c), (r, c + 1), (r, c - 1)]

            for nr, nc in neighbors:
                if (
                    0 <= nr < rows
                    and 0 <= nc < cols
                    and matrix_to_check[nr, nc] == target_color_value
                    and (nr, nc) not in visited
                ):
                    visited.add((nr, nc))
                    filled_coords.append((nr, nc))
                    q.append((nr, nc))
        return filled_coords

    def _draw_curve_on_matrix(
        self, matrix, curve_obj, origin_coord, t_span, color_value, mode=None
    ):
        """
        Draws a parametric curve stroke on the matrix.
        The curve is calculated relative to (0,0) and then translated to origin_coord.
        't_relative' is used as the parameter for the curve, effectively mapping to x-coordinates.
        The curve's intersection with the seed depends on the 'mode'.
        """
        curve_method = curve_obj.method
        origin_row, origin_col = origin_coord

        density_factor = 5
        num_points = max(2, t_span * density_factor)

        # Random scaling for visual variety based on curve type (defined once per stroke)
        scale_y = (
            random.uniform(0.01, 0.05)
            if curve_obj.name == "parabola"
            else (
                random.uniform(5, 15)
                if curve_obj.name == "sigmoid"
                else random.uniform(3, 7)
            )
        )

        # Generate base t_values and y_values
        t_values_base = np.linspace(0, t_span - 1, num_points)
        y_values_base = np.array([curve_method(t, scale_y) for t in t_values_base])

        # Determine the offset based on the mode
        if mode == "skin-default":
            # Choose a random t_relative value within the curve's span to align with the origin
            random_t_at_origin = random.uniform(0, t_span - 1)
            t_offset = random_t_at_origin
            y_offset = curve_method(random_t_at_origin, scale_y)
        # For 'skin-default-0' mode, 'skin-default-1' mode, 'skin-default-2' mode, or any other mode (including None),
        # the curve starts/originates from the seed's coordinates (t_offset=0, y_offset=0)
        else:  # mode is None, 'skin-default-0', 'skin-default-1', 'skin-default-2'
            t_offset = 0
            y_offset = 0

        for i in range(len(t_values_base)):
            t_rel = t_values_base[i]
            y_rel = y_values_base[i]

            # Translate relative coordinates to absolute matrix coordinates
            # Shift the curve so the chosen intersection point aligns with origin_col, origin_row
            abs_col = int(round(origin_col + (t_rel - t_offset)))
            abs_row = int(round(origin_row + (y_rel - y_offset)))

            if 0 <= abs_row < self.matrix_rows and 0 <= abs_col < self.matrix_cols:
                matrix[abs_row, abs_col] = color_value

    def generate_single_graph_data(
        self, graph_id, hair_seed_range, specific_fill_coords_xy, mode=None
    ):
        """
        Generates all data for a single pixel art graph, including seed positions,
        parametric curve strokes, and summary information.
        """
        current_matrix = np.copy(self.base_matrix)

        hair_seeds = self._get_random_hair_seeds(hair_seed_range[0], hair_seed_range[1])
        hair_coords = {seed.get_coordinates() for seed in hair_seeds}
        fill_seeds = self._get_fill_seeds(specific_fill_coords_xy, hair_coords, mode)

        all_seeds = hair_seeds + fill_seeds

        # Initialize color map and index map for this specific graph
        plot_specific_colors = (
            BASE_COLORS.tolist()
        )  # Always starts with white (0) and black (1)
        color_to_index_map = {
            tuple(color): i for i, color in enumerate(plot_specific_colors)
        }

        # Add skin tones to the color map
        for color in self.skin_tone_colors:
            color_tuple = tuple(color)
            if color_tuple not in color_to_index_map:
                color_to_index_map[color_tuple] = len(plot_specific_colors)
                plot_specific_colors.append(color)

        # Determine stroke color (random for hair curves) and add to color map
        stroke_color = (
            self._generate_random_rgba_color()
        )  # This is now a distinct color for debugging
        stroke_color_tuple = tuple(stroke_color)
        if stroke_color_tuple not in color_to_index_map:
            color_to_index_map[stroke_color_tuple] = len(plot_specific_colors)
            plot_specific_colors.append(stroke_color)
        stroke_color_index = color_to_index_map[stroke_color_tuple]

        # Black color index is always 1
        black_color_index = 1

        current_cmap = plt.cm.colors.ListedColormap(plot_specific_colors)

        # Only choose a curve object if there are hair seeds to draw curves
        chosen_curve_obj = None
        if hair_seeds:
            chosen_curve_obj = random.choice(list(self.parametric_curve_types.values()))
        else:
            print(
                f"Graph ID {graph_id}: No hair seeds generated, skipping curve drawing."
            )

        hair_seed_count = 0
        fill_seed_count = 0
        fill_gradient_shadow_seed_count = 0

        # --- DRAW PARAMETRIC CURVES FIRST (from hair seeds) ---
        if (
            chosen_curve_obj
        ):  # Only draw curves if a curve object was chosen (i.e., hair seeds exist)
            for seed in hair_seeds:
                hair_seed_count += 1
                self._curve_instance_id_counter += 1
                parametric_curve_instance_id = self._curve_instance_id_counter

                self._draw_curve_on_matrix(
                    current_matrix,
                    chosen_curve_obj,
                    seed.get_coordinates(),
                    t_span=random.randint(10, 25),
                    color_value=stroke_color_index,
                    mode=mode,
                )
                parent_coords = seed.get_coordinates()
                self.all_curve_instance_summaries.append(
                    {
                        "Graph ID": graph_id,
                        "Parametric Curve Instance ID": parametric_curve_instance_id,
                        "Parametric Curve": chosen_curve_obj.name.capitalize(),
                        "Domain Type": chosen_curve_obj.domain_type,
                        "Codomain Type": chosen_curve_obj.codomain_type,
                        "Parent Seed Type": seed.type,
                        "Parent Seed Coord": f"({parent_coords[1]}, {parent_coords[0]})",
                    }
                )

        # --- Draw straight black lines for skin-default-2 mode ---
        if mode == "skin-default-2":
            # TODO: this is a parametric type 'line'
            # Line 1: from (col 9, row 17) to (col 15, row 17)
            for c in range(9, 16):  # columns 9 to 15 inclusive
                if 0 <= 17 < self.matrix_rows and 0 <= c < self.matrix_cols:
                    current_matrix[17, c] = black_color_index
            # Line 2: from (col 8, row 20) to (col 16, row 20)
            for c in range(8, 17):  # columns 8 to 16 inclusive
                if 0 <= 20 < self.matrix_rows and 0 <= c < self.matrix_cols:
                    current_matrix[20, c] = black_color_index

        # --- THEN PERFORM FLOOD FILL FOR FILL SEEDS ---
        for seed in fill_seeds:
            start_row, start_col = seed.get_coordinates()

            # Get the color at the seed's position *after* curves and lines have been drawn
            target_color_value_for_fill = current_matrix[start_row, start_col]

            if seed.type == "fill-gradient-shadow":
                fill_gradient_shadow_seed_count += 1
                # Only apply gradient/shadow if the seed falls on a parametric curve's color
                # This condition now correctly checks if the pixel is the stroke color,
                # which implies a curve was drawn.
                if target_color_value_for_fill == stroke_color_index:
                    # If it IS on a parametric curve, then perform flood fill on that curve's color
                    filled_coords = self._flood_fill_get_coords(
                        current_matrix,
                        start_row,
                        start_col,
                        target_color_value_for_fill,
                    )

                    if not filled_coords:
                        continue

                    # Determine min/max coordinates for gradient calculation
                    min_row_filled = min(r for r, c in filled_coords)
                    max_row_filled = max(r for r, c in filled_coords)
                    min_col_filled = min(c for r, c in filled_coords)
                    max_col_filled = max(c for r, c in filled_coords)

                    # Use the actual parametric curve color as the base for the gradient
                    base_color_for_gradient = plot_specific_colors[stroke_color_index]

                    # Randomly choose gradient direction for skin-default-2 mode
                    gradient_direction = (
                        random.choice(
                            [
                                "left-to-right",
                                "right-to-left",
                                "top-to-bottom",
                                "bottom-to-top",
                            ]
                        )
                        if mode == "skin-default-2"
                        else "left-to-right"
                    )

                    for r, c in filled_coords:
                        darkening_factor = 0
                        noise_amount = 0

                        if gradient_direction == "left-to-right":
                            if max_col_filled == min_col_filled:
                                normalized_coord = 0
                            else:
                                normalized_coord = (c - min_col_filled) / (
                                    max_col_filled - min_col_filled
                                )
                            darkening_factor = normalized_coord * 0.5
                            noise_amount = normalized_coord * 0.1 * random.random()
                        elif gradient_direction == "right-to-left":
                            if max_col_filled == min_col_filled:
                                normalized_coord = 0
                            else:
                                normalized_coord = (max_col_filled - c) / (
                                    max_col_filled - min_col_filled
                                )
                            darkening_factor = normalized_coord * 0.5
                            noise_amount = normalized_coord * 0.1 * random.random()
                        elif gradient_direction == "top-to-bottom":
                            if max_row_filled == min_row_filled:
                                normalized_coord = 0
                            else:
                                normalized_coord = (r - min_row_filled) / (
                                    max_row_filled - min_row_filled
                                )
                            darkening_factor = normalized_coord * 0.5
                            noise_amount = normalized_coord * 0.1 * random.random()
                        elif gradient_direction == "bottom-to-top":
                            if max_row_filled == min_row_filled:
                                normalized_coord = 0
                            else:
                                normalized_coord = (max_row_filled - r) / (
                                    max_row_filled - min_row_filled
                                )
                            darkening_factor = normalized_coord * 0.5
                            noise_amount = normalized_coord * 0.1 * random.random()

                        darkened_rgba = [
                            max(0, base_color_for_gradient[0] * (1 - darkening_factor)),
                            max(0, base_color_for_gradient[1] * (1 - darkening_factor)),
                            max(0, base_color_for_gradient[2] * (1 - darkening_factor)),
                            base_color_for_gradient[3],
                        ]

                        final_rgba = [
                            max(0, darkened_rgba[0] - noise_amount),
                            max(0, darkened_rgba[1] - noise_amount),
                            max(0, darkened_rgba[2] - noise_amount),
                            darkened_rgba[3],
                        ]

                        final_rgba_tuple = tuple(final_rgba)

                        if final_rgba_tuple not in color_to_index_map:
                            color_to_index_map[final_rgba_tuple] = len(
                                plot_specific_colors
                            )
                            plot_specific_colors.append(list(final_rgba_tuple))

                        current_matrix[r, c] = color_to_index_map[final_rgba_tuple]
                else:
                    # If not on a curve (i.e., on white/black background), skip filling
                    continue

            else:  # Regular fill seed (type == "fill")
                fill_seed_count += 1
                # For regular fill seeds, they should fill the contiguous area of the ORIGINAL white background (0).
                # They should never fill black pixels (1).

                # Check if the starting point is an original black pixel. If so, skip.
                # This check remains valid for preventing fills over original silhouette pixels.
                if self.base_matrix[start_row, start_col] == 1:
                    continue

                # The target for flood fill for a regular FillSeed should always be the white background (0).
                target_color_for_flood_fill = 0

                filled_coords = self._flood_fill_get_coords(
                    current_matrix, start_row, start_col, target_color_for_flood_fill
                )

                if not filled_coords:
                    continue

                # Debug print to show the mode for this fill seed
                print(f"DEBUG: Graph ID {graph_id}, Mode for fill seed: {mode}")

                # Determine fill color for FillSeed (F)
                if target_color_value_for_fill == 0:  # If starting on a white pixel
                    # User wants random skin tones for default fills
                    if hair_seed_range[1] > 0:
                        base_skin_tone_rgba = random.choice(self.skin_tone_colors)
                        base_skin_tone_tuple = tuple(base_skin_tone_rgba)
                        if base_skin_tone_tuple not in color_to_index_map:
                            color_to_index_map[base_skin_tone_tuple] = len(
                                plot_specific_colors
                            )
                            plot_specific_colors.append(list(base_skin_tone_tuple))
                        fill_color_index = color_to_index_map[base_skin_tone_tuple]
                        print(
                            f"DEBUG: Graph ID {graph_id}, Chosen skin tone (random for white area): {base_skin_tone_rgba}"
                        )
                    else:
                        fill_color_index = stroke_color_index
                elif (
                    target_color_value_for_fill == stroke_color_index
                    and chosen_curve_obj
                ):  # If starting on a parametric curve color
                    # User wants to extend the parametric curve color
                    fill_color_index = stroke_color_index
                    print(
                        f"DEBUG: Graph ID {graph_id}, Chosen fill color (extending curve): {plot_specific_colors[fill_color_index]}"
                    )
                else:
                    # If it's neither white nor a curve color, do nothing (shouldn't happen often if logic is sound)
                    continue

                for r, c in filled_coords:
                    # Crucial check: only fill if the pixel is currently white (0) or the target curve color
                    # This prevents filling over original silhouette or other distinct elements.
                    # The condition is now more flexible to allow filling over the curve color if that's the target.
                    if (
                        current_matrix[r, c] == target_color_value_for_fill
                    ):  # Fill only if it matches the color we started on
                        current_matrix[r, c] = fill_color_index

        # After all fills, update the colormap with all potentially new colors
        current_cmap = plt.cm.colors.ListedColormap(plot_specific_colors)

        self.graph_seed_counts_summaries.append(
            {
                "Graph ID": graph_id,
                "Hair Seeds Count": hair_seed_count,
                "Fill Seeds Count": fill_seed_count,
                "Fill Gradient Shadow Seeds Count": fill_gradient_shadow_seed_count,
            }
        )

        return current_matrix, current_cmap, all_seeds

    def render_graphs(
        self,
        num_graphs=8,
        hair_seed_range=(0, 0),
        specific_fill_coords_xy=None,
        mode=None,
    ):
        """Generates and displays multiple pixel art graphs with summaries."""
        graph_data_list = []
        for i in range(num_graphs):
            graph_data_list.append(
                self.generate_single_graph_data(
                    i + 1, hair_seed_range, specific_fill_coords_xy, mode
                )
            )

        self.all_curve_instance_summaries.sort(
            key=lambda x: (x["Graph ID"], x["Parametric Curve Instance ID"])
        )
        self.graph_seed_counts_summaries.sort(key=lambda x: x["Graph ID"])

        main_headers = [
            "Graph ID",
            "Parametric Curve Instance ID",
            "Parametric Curve",
            "Domain Type",
            "Codomain Type",
            "Parent Seed Type",
            "Parent Seed Coord",
        ]
        main_table_data = [
            [s[h] for h in main_headers] for s in self.all_curve_instance_summaries
        ]
        print("\n--- Parametric Curve Instance Details ---\n")
        # Ensure table is printed even if empty
        if main_table_data:
            print(tabulate(main_table_data, headers=main_headers, tablefmt="grid"))
        else:
            print("No parametric curve instances generated.")

        # Updated headers for the seed summary table to include character symbols
        seed_summary_headers = [
            "Graph ID",
            "Hair Seeds Count (H)",
            "Fill Seeds Count (F)",
            "Fill Gradient Shadow Seeds Count (G)",
        ]
        seed_summary_data = [
            [s[h.split("(")[0].strip()] for h in seed_summary_headers]
            for s in self.graph_seed_counts_summaries
        ]
        print("\n--- Seed Distribution Summary ---\n")
        print(
            tabulate(seed_summary_data, headers=seed_summary_headers, tablefmt="grid")
        )

        fig, axes = plt.subplots(2, 4, figsize=(20, 10))
        axes = axes.flatten()

        for i, (matrix, cmap, seeds) in enumerate(graph_data_list):
            graph_id = i + 1
            axes[i].imshow(matrix, cmap=cmap, origin="upper")
            axes[i].set_title(f"Graph ID: {graph_id}")
            axes[i].set_xticks([])
            axes[i].set_yticks([])

            for seed in seeds:
                coords = seed.get_coordinates()
                marker_props = seed.get_marker_properties()
                # Updated text to only include the character
                label_text = marker_props["s"]
                axes[i].text(
                    coords[1],
                    coords[0],
                    label_text,
                    color=marker_props["color"],
                    ha="center",
                    va="center",
                    fontsize=10,
                    fontweight="bold",
                    bbox=marker_props["bbox"],
                )

        plt.tight_layout(rect=[0, 0.03, 1, 0.95])
        plt.show()


# --- Main Execution ---
if __name__ == "__main__":
    random.seed()  # Initialize random seed once at the start for true randomness across script runs

    parser = argparse.ArgumentParser(
        description="Generate pixel art with parametric curves and seed positions."
    )
    parser.add_argument(
        "--range-hair-seeds",
        nargs="+",
        type=int,
        default=[0],
        help="Range (min, max) for random uniform number of hair seeds per graph. Provide as: min_count max_count. Default: 1",
    )
    parser.add_argument(
        "--point-fill-seeds",
        type=str,
        default=None,
        help='Specific (x,y) coordinates for fill seeds, e.g., "1-2,3-4". Default: center (1 seed).',
    )
    parser.add_argument(
        "--mode",
        type=str,
        default=None,
        help='Special mode for rendering. Current options: "skin-default", "skin-default-0", "skin-default-1", "skin-default-2".',
    )

    args = parser.parse_args()

    if len(args.range_hair_seeds) == 1:
        hair_min_seeds = args.range_hair_seeds[0]
        hair_max_seeds = args.range_hair_seeds[0]
    elif len(args.range_hair_seeds) == 2:
        hair_min_seeds = args.range_hair_seeds[0]
        hair_max_seeds = args.range_hair_seeds[1]
    else:
        raise ValueError(
            "range-hair-seeds must have 1 or 2 integer values (min_count or min_count max_count)."
        )

    hair_seed_range = (int(hair_min_seeds), int(hair_max_seeds))

    specific_fill_coords = []
    if args.point_fill_seeds:
        try:
            points_str = args.point_fill_seeds.split(",")
            for point_str in points_str:
                x_str, y_str = point_str.strip().split("-")
                specific_fill_coords.append((int(x_str), int(y_str)))
        except ValueError:
            raise ValueError(
                "Invalid format for --point-fill-seeds. Use 'x1-y1,x2-y2,...'."
            )

    generator = PixelArtGenerator(
        BASE_MATRIX,
        HAIR_SPAWN_ROWS,
        HAIR_SPAWN_COLS,
        PARAMETRIC_CURVE_TYPES,
        SKIN_TONE_COLORS,
    )
    generator.render_graphs(
        num_graphs=8,
        hair_seed_range=hair_seed_range,
        specific_fill_coords_xy=specific_fill_coords,
        mode=args.mode,
    )
