import matplotlib.pyplot as plt
import numpy as np
import random
import math

# Synthetic Data Generation (SDG) script/tool

# Original MATRIX representing the pixel art drawing
MATRIX = [
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

# Convert MATRIX to a NumPy array for easier manipulation
matrix_np = np.array(MATRIX)
matrix_rows, matrix_cols = matrix_np.shape


# Function to generate a random RGBA color
def generate_random_rgba_color():
    """Generates a random RGBA color."""
    return [random.random(), random.random(), random.random(), 1.0]


# Define MAP_COLOR_BASE: Only white and black for the base pixel art
MAP_COLOR_BASE = np.array(
    [[1.0, 1.0, 1.0, 1.0], [0.0, 0.0, 0.0, 1.0]]  # Index 0: White  # Index 1: Black
)

# Define the region "above the head" for seed positions
ABOVE_HEAD_ROWS_RANGE = (0, 3)
ABOVE_HEAD_COLS_RANGE = (5, 20)  # Expanded range for horizontal dispersion


def get_random_seed_positions(rows_range, cols_range, min_dots, max_dots):
    """
    Generates a random number of seed positions (coordinates) within the specified region.

    Args:
        rows_range (tuple): A tuple (start_row, end_row) defining the vertical range.
        cols_range (tuple): A tuple (start_col, end_col) defining the horizontal range.
        min_dots (int): Minimum number of seed positions to generate.
        max_dots (int): Maximum number of seed positions to generate.

    Returns:
        list: A list of (row, col) tuples for the seed positions.
    """
    num_dots = random.randint(min_dots, max_dots)

    possible_coords = []
    for r in range(rows_range[0], rows_range[1] + 1):
        for c in range(cols_range[0], cols_range[1] + 1):
            possible_coords.append((r, c))

    num_dots = min(num_dots, len(possible_coords))
    selected_coords = random.sample(possible_coords, num_dots)
    return selected_coords


def draw_math_function_fragment_intersecting(
    matrix, func_type, target_coord, x_span, color_value
):
    """
    Draws a fragment of a mathematical function onto the matrix, ensuring it intersects
    the target_coord.

    Args:
        matrix (np.array): The input pixel art matrix.
        func_type (str): Type of function ('parabola', 'sigmoid', 'sine').
        target_coord (tuple): (target_row, target_col) - the coordinate the function must intersect.
        x_span (int): The total horizontal span of the function fragment.
        color_value (int): The integer value to set pixels to for this trace's color.
    """
    target_row, target_col = target_coord
    matrix_rows, matrix_cols = matrix.shape

    # Define a random starting X for the fragment, ensuring target_col is within the span
    start_x = random.randint(
        max(0, target_col - x_span + 1), min(matrix_cols - x_span, target_col)
    )
    end_x = start_x + x_span - 1

    # Adjust parameters to ensure intersection
    for x in range(start_x, end_x + 1):
        y = 0
        if func_type == "parabola":
            center_x = target_col
            offset_y = target_row
            scale_y = random.uniform(0.01, 0.05)
            y = scale_y * ((x - center_x) ** 2) + offset_y
        elif func_type == "sigmoid":
            amplitude = random.uniform(8, 15)
            steepness = random.uniform(0.5, 1.5)
            x0 = target_col
            y_base = target_row - 0.5 * amplitude
            scaled_x = (x - x0) * steepness
            y = amplitude / (1 + math.exp(-scaled_x)) + y_base
        elif func_type == "sine":
            amplitude = random.uniform(3, 7)
            frequency = random.uniform(0.3, 0.8)
            phase_shift_x = target_col
            vertical_shift = target_row
            y = amplitude * math.sin(frequency * (x - phase_shift_x)) + vertical_shift
        else:
            continue

        row = int(round(y))
        col = int(round(x))

        if 0 <= row < matrix_rows and 0 <= col < matrix_cols:
            matrix[row, col] = color_value


# Generate and display 8 examples in a 2x4 grid
fig, axes = plt.subplots(2, 4, figsize=(20, 10))  # 2 rows, 4 columns
fig.suptitle(
    "Pixel Art Drawing with Dynamic Colors and Intersecting Paths", fontsize=16
)

# Flatten the axes array for easy iteration
axes = axes.flatten()

# Define available function types
function_types = ["parabola", "sigmoid", "sine"]

for i in range(8):
    # Start with a fresh copy of the base matrix for each example
    current_matrix = np.copy(matrix_np)

    # Get random seed positions (these are not drawn into the matrix yet)
    random.seed(i + 200)  # Seed for red dot positions
    seed_positions = get_random_seed_positions(
        ABOVE_HEAD_ROWS_RANGE, ABOVE_HEAD_COLS_RANGE, min_dots=3, max_dots=8
    )
    random.shuffle(
        seed_positions
    )  # Shuffle to ensure random assignment for each function

    # Create a plot-specific MAP_COLOR list
    plot_specific_map_color_list = MAP_COLOR_BASE.tolist()

    # Generate a single random color for all mathematical paths in this plot
    random_math_path_color = generate_random_rgba_color()
    plot_specific_map_color_list.append(random_math_path_color)
    math_color_value = (
        len(plot_specific_map_color_list) - 1
    )  # This will be the index for the math path color

    # Create the colormap for this specific plot
    current_cmap = plt.cm.colors.ListedColormap(plot_specific_map_color_list)

    # Draw mathematical function fragments
    random.seed(i + 300)  # Seed for function types and spans

    for j, target_dot in enumerate(seed_positions):
        func_type = random.choice(function_types)
        draw_math_function_fragment_intersecting(
            current_matrix,
            func_type,
            target_dot,
            x_span=random.randint(10, 25),  # Random span for variety
            color_value=math_color_value,  # Use the single random color for this plot
        )

    # Plot the matrix
    axes[i].imshow(current_matrix, cmap=current_cmap, origin="upper")
    axes[i].set_title(f"Example {i+1}")
    axes[i].set_xticks([])
    axes[i].set_yticks([])

    # Draw 'x' text for seed positions on top with a background
    for r, c in seed_positions:
        axes[i].text(
            c,
            r,
            "x",
            color="red",  # The color of the 'x' itself
            ha="center",
            va="center",
            fontsize=10,
            fontweight="bold",
            bbox=dict(
                facecolor="white", alpha=0.7, edgecolor="none", boxstyle="round,pad=0.2"
            ),  # Added background
        )


plt.tight_layout(rect=[0, 0.03, 1, 0.95])
plt.show()
