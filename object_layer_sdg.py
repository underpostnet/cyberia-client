# Synthetic Data Generation (SDG) script/tool

import matplotlib.pyplot as plt
import numpy as np
import random
import math
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
HAIR_SPAWN_ROWS = (0, 3)
HAIR_SPAWN_COLS = (5, 20)


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
        raise NotImplementedError("Subclasses must implement this method.")


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
        super().__init__(row, col, "x", "black", "lightgray")
        self.type = "fill"

    def can_generate_stroke(self):
        return False


# --- Pixel Art Generation Logic ---
class PixelArtGenerator:
    """Generates and renders pixel art with dynamic elements and provides summaries."""

    def __init__(
        self, base_matrix_data, hair_spawn_rows, hair_spawn_cols, parametric_curve_types
    ):
        self.base_matrix = np.array(base_matrix_data)
        self.matrix_rows, self.matrix_cols = self.base_matrix.shape
        self.hair_spawn_rows = hair_spawn_rows
        self.hair_spawn_cols = hair_spawn_cols
        self.parametric_curve_types = parametric_curve_types
        self.graph_summaries = []

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

    def _get_center_fill_seed(self):
        """Generates the 'fill' seed position at the center of the matrix."""
        return FillSeed(self.matrix_rows // 2, self.matrix_cols // 2)

    def _draw_curve_on_matrix(
        self, matrix, curve_obj, origin_coord, t_span, color_value
    ):
        """
        Draws a parametric curve stroke on the matrix.
        The curve is calculated relative to (0,0) and then translated to origin_coord.
        't_relative' is used as the parameter for the curve, effectively mapping to x-coordinates.
        """
        curve_method = curve_obj.method
        origin_row, origin_col = origin_coord

        t_start_relative = 0
        t_end_relative = t_span - 1

        # Random scaling for visual variety based on curve type
        scale_y = (
            random.uniform(0.01, 0.05)
            if curve_obj.name == "parabola"
            else (
                random.uniform(5, 15)
                if curve_obj.name == "sigmoid"
                else random.uniform(3, 7)
            )
        )  # Sine

        for t_rel in range(t_start_relative, t_end_relative + 1):
            y_rel = curve_method(t_rel, scale_y)

            # Translate relative coordinates to absolute matrix coordinates
            abs_row = int(round(origin_row + y_rel))
            abs_col = int(round(origin_col + t_rel))  # t_relative maps to x-coordinate

            # Ensure coordinates are within matrix bounds
            if 0 <= abs_row < self.matrix_rows and 0 <= abs_col < self.matrix_cols:
                matrix[abs_row, abs_col] = color_value

    def generate_single_graph_data(self, graph_id):
        """
        Generates all data for a single pixel art graph, including seed positions,
        parametric curve strokes, and summary information.
        """
        current_matrix = np.copy(self.base_matrix)

        # Generate seed positions
        random.seed(graph_id + 200)
        hair_seeds = self._get_random_hair_seeds(min_count=3, max_count=8)
        fill_seed = self._get_center_fill_seed()

        all_seeds = hair_seeds + [fill_seed]
        random.shuffle(all_seeds)

        # Prepare plot-specific color map
        plot_specific_colors = BASE_COLORS.tolist()
        stroke_color = self._generate_random_rgba_color()
        plot_specific_colors.append(stroke_color)
        stroke_color_value = len(plot_specific_colors) - 1
        current_cmap = plt.cm.colors.ListedColormap(plot_specific_colors)

        # Choose one random parametric curve type for all strokes in this graph
        random.seed(graph_id + 300)
        chosen_curve_obj = random.choice(list(self.parametric_curve_types.values()))

        hair_seed_count = 0
        fill_seed_count = 0

        # Draw parametric curve strokes for eligible seed positions
        for seed in all_seeds:
            if seed.type == "hair":
                hair_seed_count += 1
            elif seed.type == "fill":
                fill_seed_count += 1

            if seed.can_generate_stroke():
                self._draw_curve_on_matrix(
                    current_matrix,
                    chosen_curve_obj,
                    seed.get_coordinates(),
                    t_span=random.randint(10, 25),  # t_span for the parametric curve
                    color_value=stroke_color_value,
                )

        # Store summary data for the current graph
        self.graph_summaries.append(
            {
                "Graph ID": graph_id,
                "Parametric Curve": chosen_curve_obj.name.capitalize(),
                "Domain Type": chosen_curve_obj.domain_type,
                "Codomain Type": chosen_curve_obj.codomain_type,
                "Hair Seeds Count": hair_seed_count,
                "Fill Seeds Count": fill_seed_count,
            }
        )

        return current_matrix, current_cmap, all_seeds

    def render_graphs(self, num_graphs=8):
        """Generates and displays multiple pixel art graphs with summaries."""
        # Generate all graph data first
        graph_data_list = []
        for i in range(num_graphs):
            graph_data_list.append(self.generate_single_graph_data(i + 1))

        # Sort graph_summaries by 'Graph ID' for consistent table output
        self.graph_summaries.sort(key=lambda x: x["Graph ID"])

        # Print the summary table to the terminal
        headers = [
            "Graph ID",
            "Parametric Curve",
            "Domain Type",
            "Codomain Type",
            "Hair Seeds Count",
            "Fill Seeds Count",
        ]
        table_data = [[s[h] for h in headers] for s in self.graph_summaries]
        print(tabulate(table_data, headers=headers, tablefmt="grid"))

        # Now display the plots
        fig, axes = plt.subplots(2, 4, figsize=(20, 10))
        fig.suptitle(
            "Pixel Art Drawing with Seed Position Types and Dynamic Paths", fontsize=16
        )
        axes = axes.flatten()

        for i, (matrix, cmap, seeds) in enumerate(graph_data_list):
            graph_id = i + 1
            axes[i].imshow(matrix, cmap=cmap, origin="upper")
            axes[i].set_title(f"Graph ID: {graph_id}")
            axes[i].set_xticks([])
            axes[i].set_yticks([])

            # Draw 'x' markers for seed positions
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
    generator = PixelArtGenerator(
        BASE_MATRIX, HAIR_SPAWN_ROWS, HAIR_SPAWN_COLS, PARAMETRIC_CURVE_TYPES
    )
    generator.render_graphs(num_graphs=8)
