import matplotlib.pyplot as plt
import numpy as np
import random

from object_layer.object_layer_data import DEFAULT_PLAYER_SKIN_FRAME_DOWN_IDLE

# Convert the imported list to a NumPy array
DEFAULT_PLAYER_SKIN_FRAME_DOWN_IDLE = np.array(DEFAULT_PLAYER_SKIN_FRAME_DOWN_IDLE)


# Function to convert RGBA (0-255) to Matplotlib's RGBA (0-1)
def rgba_to_mpl_color(r, g, b, a):
    """Converts RGBA color values from 0-255 range to 0-1 range."""
    return (r / 255.0, g / 255.0, b / 255.0, a / 255.0)


# Define the color mapping for 0 (white) and 1 (black) in RGBA (0-255) format
map_color = {
    0: (255, 255, 255, 255),  # White (R, G, B, Alpha)
    1: (0, 0, 0, 255),  # Black (R, G, B, Alpha)
}

# Get the dimensions of the pixel art matrix
MATRIX_HEIGHT, MATRIX_WIDTH = DEFAULT_PLAYER_SKIN_FRAME_DOWN_IDLE.shape

# Create a figure and a 2x4 grid of subplots
# figsize is set to a larger value to ensure each 26x26 grid is clearly visible.
# dpi is set to 100, meaning 100 pixels per inch.
# With figsize=(26, 13) and dpi=100, each subplot (approx 6.5x6.5 inches)
# will be rendered at roughly 650x650 pixels, making each of the 26x26 cells
# about 25x25 screen pixels, ensuring high visibility.
fig, axes = plt.subplots(2, 4, figsize=(26, 13), dpi=100)

# Flatten the axes array for easy iteration
axes = axes.flatten()

# Configure each subplot to display the pixel art on a 26x26 grid
for i, ax in enumerate(axes):
    # Set the limits for the x and y axes to define the 26x26 area
    # These limits define the coordinate system for drawing the squares.
    ax.set_xlim(0, MATRIX_WIDTH)
    ax.set_ylim(0, MATRIX_HEIGHT)

    # Set axis labels to indicate pixel units
    ax.set_xlabel("Pixel X")
    ax.set_ylabel("Pixel Y")

    # Ensure the aspect ratio is equal, so each "pixel" cell is square
    ax.set_aspect("equal", adjustable="box")

    # Iterate over the matrix to draw each pixel as a colored square using ax.fill
    for row_idx in range(MATRIX_HEIGHT):
        for col_idx in range(MATRIX_WIDTH):
            pixel_value = DEFAULT_PLAYER_SKIN_FRAME_DOWN_IDLE[row_idx, col_idx]

            # Get the RGBA (0-255) color from the map
            rgba_255 = map_color[pixel_value]

            # Convert to Matplotlib's RGBA (0-1) format
            color = rgba_to_mpl_color(*rgba_255)

            # Calculate the coordinates for the current pixel's square
            # (x, y) specifies the bottom-left corner of the square.
            # To render rows correctly (0 at top, 25 at bottom),
            # we invert the y-coordinate for drawing.
            # So, row_idx 0 (top of matrix) maps to y = MATRIX_HEIGHT - 1 (top of plot).
            # row_idx 25 (bottom of matrix) maps to y = 0 (bottom of plot).
            square_y_bottom = MATRIX_HEIGHT - 1 - row_idx
            square_x_left = col_idx

            # Define the four corners of the square
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

            # Use ax.fill to draw the colored square.
            # edgecolor='none' prevents drawing a border around each pixel,
            # which would interfere with the grid lines.
            ax.fill(x_coords, y_coords, color=color, edgecolor="none")

    # Draw horizontal grid lines on top of the pixels
    for y in range(
        MATRIX_HEIGHT + 1
    ):  # Go up to MATRIX_HEIGHT to draw the line at y=MATRIX_HEIGHT
        ax.axhline(y, color="lightgray", linestyle="-", linewidth=0.5)

    # Draw vertical grid lines on top of the pixels
    for x in range(
        MATRIX_WIDTH + 1
    ):  # Go up to MATRIX_WIDTH to draw the line at x=MATRIX_WIDTH
        ax.axvline(x, color="lightgray", linestyle="-", linewidth=0.5)

# Adjust layout to prevent titles/labels from overlapping

plt.tight_layout()

# Display the plot
plt.show()
