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
        super().__init__(row, col, "x", "red", "white")
        self.type = "hair"

    def can_generate_stroke(self):
        return True


class FillSeed(SeedPosition):
    """Represents a 'fill' type seed position, which does not generate a parametric curve stroke."""

    def __init__(self, row, col):
        super().__init__(row, col, "x", "black", "none")
        self.type = "fill"

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

    def _get_fill_seeds(self, specific_coords_xy, existing_hair_coords):
        """
        Generates 'fill' seed positions based on specific (x,y) coordinates provided.
        If specific_coords_xy is empty, it defaults to a single fill seed at the center.
        """
        fill_seeds = []
        if not specific_coords_xy:
            fill_seeds.append(FillSeed(self.matrix_rows // 2, self.matrix_cols // 2))
        else:
            for x, y in specific_coords_xy:
                if 0 <= y < self.matrix_rows and 0 <= x < self.matrix_cols:
                    fill_seeds.append(FillSeed(y, x))
                else:
                    print(
                        f"Warning: Fill seed coordinate ({x},{y}) is out of matrix bounds and will be skipped."
                    )
        return fill_seeds

    def _flood_fill(
        self, matrix, start_row, start_col, target_color_value, fill_color_value
    ):
        """
        Performs a flood fill (paint bucket) operation on the matrix.
        Changes contiguous pixels of target_color_value to fill_color_value.
        """
        rows, cols = matrix.shape

        if not (0 <= start_row < rows and 0 <= start_col < cols):
            return

        if (
            matrix[start_row, start_col] != target_color_value
            or matrix[start_row, start_col] == fill_color_value
        ):
            return

        q = [(start_row, start_col)]
        matrix[start_row, start_col] = fill_color_value

        while q:
            r, c = q.pop(0)

            neighbors = [(r + 1, c), (r - 1, c), (r, c + 1), (r, c - 1)]

            for nr, nc in neighbors:
                if (
                    0 <= nr < rows
                    and 0 <= nc < cols
                    and matrix[nr, nc] == target_color_value
                ):
                    matrix[nr, nc] = fill_color_value
                    q.append((nr, nc))

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
            # Choose a random point on the curve to intersect the seed
            idx_at_seed = random.randint(0, len(t_values_base) - 1)
            t_offset = t_values_base[idx_at_seed]
            y_offset = y_values_base[idx_at_seed]
        # For 'skin-default-0' mode or any other mode (including None),
        # the curve starts/originates from the seed's coordinates
        else:
            t_offset = 0
            y_offset = 0

        for i in range(len(t_values_base)):
            t_rel = t_values_base[i]
            y_rel = y_values_base[i]

            # Translate relative coordinates to absolute matrix coordinates
            # Shift the curve so the chosen intersection point aligns with origin_col, origin_row
            abs_col = int(round(origin_col + (t_rel - t_offset)))
            abs_row = int(round(origin_row + (y_rel - y_offset)))

            # Ensure coordinates are within matrix bounds
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

        fill_seeds = self._get_fill_seeds(specific_fill_coords_xy, hair_coords)

        all_seeds = hair_seeds + fill_seeds
        random.shuffle(all_seeds)

        plot_specific_colors = BASE_COLORS.tolist()

        skin_tone_start_index = len(plot_specific_colors)
        for color in self.skin_tone_colors:
            plot_specific_colors.append(color)

        stroke_color = self._generate_random_rgba_color()
        stroke_color_index = len(plot_specific_colors)
        plot_specific_colors.append(stroke_color)

        current_cmap = plt.cm.colors.ListedColormap(plot_specific_colors)

        chosen_curve_obj = random.choice(list(self.parametric_curve_types.values()))

        hair_seed_count = 0
        fill_seed_count = 0

        # --- DRAW PARAMETRIC CURVES FIRST (from hair seeds) ---
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
                mode=mode,  # Pass the mode to the drawing function
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

        # --- THEN PERFORM FLOOD FILL FOR FILL SEEDS ---
        for seed in fill_seeds:
            fill_seed_count += 1
            start_row, start_col = seed.get_coordinates()
            target_color_value_at_seed = current_matrix[start_row, start_col]

            fill_color_index_for_this_fill = random.randint(
                len(BASE_COLORS), len(BASE_COLORS) + len(self.skin_tone_colors) - 1
            )

            self._flood_fill(
                current_matrix,
                start_row,
                start_col,
                target_color_value_at_seed,
                fill_color_index_for_this_fill,
            )

        self.graph_seed_counts_summaries.append(
            {
                "Graph ID": graph_id,
                "Hair Seeds Count": hair_seed_count,
                "Fill Seeds Count": fill_seed_count,
            }
        )

        return current_matrix, current_cmap, all_seeds

    def render_graphs(
        self,
        num_graphs=8,
        hair_seed_range=(1, 1),
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
        print(tabulate(main_table_data, headers=main_headers, tablefmt="grid"))

        seed_summary_headers = ["Graph ID", "Hair Seeds Count", "Fill Seeds Count"]
        seed_summary_data = [
            [s[h] for h in seed_summary_headers]
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
                axes[i].text(
                    coords[1],
                    coords[0],
                    marker_props["s"],
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
        default=[1],
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
        help='Special mode for rendering. Current options: "skin-default", "skin-default-0".',
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

    hair_seed_range = (hair_min_seeds, hair_max_seeds)

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
