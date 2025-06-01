import matplotlib.pyplot as plt
import numpy as np
import random

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

# Define MAP_COLOR:
# Index 0: White (for 0 in matrix)
# Index 1: Black (for 1 in matrix)
# Index 2: Red (for 2 in matrix, representing the 'red dots')
MAP_COLOR = np.array(
    [
        [1.0, 1.0, 1.0, 1.0],  # White (RGBA)
        [0.0, 0.0, 0.0, 1.0],  # Black (RGBA)
        [1.0, 0.0, 0.0, 1.0],  # Red (RGBA)
    ]
)

# Define the region "above the head" for red dots
# Rows 0-3 (inclusive)
# Expanded columns to allow points to be "half outside" the original head limits (9-17)
# Let's say from column 5 to 20 to give it more room.
ABOVE_HEAD_ROWS_RANGE = (0, 3)
ABOVE_HEAD_COLS_RANGE = (5, 20)  # Expanded range for horizontal dispersion


def add_random_red_dots_above_head(matrix, rows_range, cols_range, min_dots, max_dots):
    """
    Adds a random number of red dots (value 2) to the specified region of the matrix.

    Args:
        matrix (np.array): The input pixel art matrix.
        rows_range (tuple): A tuple (start_row, end_row) defining the vertical range.
        cols_range (tuple): A tuple (start_col, end_col) defining the horizontal range.
        min_dots (int): Minimum number of red dots to add.
        max_dots (int): Maximum number of red dots to add.

    Returns:
        np.array: A new matrix with red dots added.
    """
    modified_matrix = np.copy(matrix)
    num_dots = random.randint(min_dots, max_dots)

    # Get possible coordinates within the defined range
    possible_coords = []
    for r in range(rows_range[0], rows_range[1] + 1):
        for c in range(cols_range[0], cols_range[1] + 1):
            possible_coords.append((r, c))

    # Ensure we don't try to place more dots than available unique positions
    num_dots = min(num_dots, len(possible_coords))

    # Randomly select unique coordinates for the dots
    selected_coords = random.sample(possible_coords, num_dots)

    for r, c in selected_coords:
        # Ensure coordinates are within the actual matrix bounds
        if 0 <= r < modified_matrix.shape[0] and 0 <= c < modified_matrix.shape[1]:
            modified_matrix[r, c] = 2  # Set pixel to red

    return modified_matrix


# Generate and display 8 examples in a 2x4 grid
fig, axes = plt.subplots(2, 4, figsize=(20, 10))  # 2 rows, 4 columns
fig.suptitle(
    "Pixel Art Drawing with Random Red Dots Above Head (Expanded)", fontsize=16
)

# Flatten the axes array for easy iteration
axes = axes.flatten()

for i in range(8):
    # Create a modified matrix for each example
    random.seed(i + 200)  # Using different seeds for distinct random outcomes

    # Add random red dots above the head (3 to 8 dots)
    modified_matrix = add_random_red_dots_above_head(
        matrix_np,
        ABOVE_HEAD_ROWS_RANGE,
        ABOVE_HEAD_COLS_RANGE,
        min_dots=3,  # Changed to 3
        max_dots=8,  # Changed to 8
    )

    # Plot the matrix
    axes[i].imshow(
        modified_matrix, cmap=plt.cm.colors.ListedColormap(MAP_COLOR), origin="upper"
    )
    axes[i].set_title(f"Example {i+1}")
    axes[i].set_xticks([])
    axes[i].set_yticks([])

plt.tight_layout(rect=[0, 0.03, 1, 0.95])
plt.show()
