import matplotlib.pyplot as plt
import numpy as np
import random
from pixel_art_editor import PixelArtEditor  # Import the new class

# Import the default player skin frame. This is the authoritative matrix.
from object_layer.object_layer_data import DEFAULT_PLAYER_SKIN_FRAME_DOWN_IDLE


# Define the color mapping for 0 (white) and 1 (black) in RGBA (0-255) format
COLOR_PALETTE = {
    0: (255, 255, 255, 255),  # White (R, G, B, Alpha)
    1: (0, 0, 0, 255),  # Black (R, G, B, Alpha)
    2: (255, 0, 0, 255),  # Red (R, G, B, Alpha)
    3: (0, 255, 0, 255),  # Green (R, G, B, Alpha)
    4: (0, 0, 255, 255),  # Blue (R, G, B, Alpha)
    5: (255, 255, 0, 255),  # Yellow (R, G, B, Alpha)
    6: (255, 0, 255, 255),  # Magenta (R, G, B, Alpha)
    7: (0, 255, 255, 255),  # Cyan (R, G, B, Alpha)
}

# Initialize the PixelArtEditor with the authoritative DEFAULT_PLAYER_SKIN_FRAME_DOWN_IDLE
# We use .copy() to ensure the editor works on its own instance of the matrix,
# preventing unintended modifications to the original imported array if it were mutable.
editor = PixelArtEditor(DEFAULT_PLAYER_SKIN_FRAME_DOWN_IDLE.copy(), COLOR_PALETTE)


# --- Demonstrate Drawing by directly overwriting pixels ---

# Example drawing operation: Draw a red pixel on the silhouette (border)
editor.draw_pixel(0, 0, 2)  # Draw red (color ID 2) at (0,0)
editor.draw_pixel(0, 1, 2)
editor.draw_pixel(1, 0, 2)
editor.draw_pixel(1, 1, 2)

# Add your requested green pixel draws *after* the previous operations
editor.draw_pixel(11, 11, 3)  # Draw green (color ID 3)
editor.draw_pixel(12, 12, 3)  # Draw green (color ID 3)
editor.draw_pixel(13, 13, 3)  # Draw green (color ID 3)

# Get the dimensions of the pixel art matrix after all operations
MATRIX_HEIGHT, MATRIX_WIDTH = editor.matrix.shape

# Create a figure and a 2x4 grid of subplots
fig, axes = plt.subplots(2, 4, figsize=(26, 13), dpi=100)

# Flatten the axes array for easy iteration
axes = axes.flatten()

# Configure each subplot to display the pixel art on a 26x26 grid
for i, ax in enumerate(axes):
    ax.set_xlim(0, MATRIX_WIDTH)
    ax.set_ylim(0, MATRIX_HEIGHT)
    ax.set_xlabel("Pixel X")
    ax.set_ylabel("Pixel Y")
    ax.set_aspect("equal", adjustable="box")
    ax.set_title(f"Pixel Art {i+1}")  # Add a title for each subplot

    # Iterate over the matrix to draw each pixel as a colored square
    for row_idx in range(MATRIX_HEIGHT):
        for col_idx in range(MATRIX_WIDTH):
            pixel_value = editor.matrix[row_idx, col_idx]
            color = editor.get_mpl_color(pixel_value)

            # Calculate the coordinates for the current pixel's square
            # Invert y-coordinate for drawing to match matrix (0 at top) to plot (0 at bottom)
            square_y_bottom = MATRIX_HEIGHT - 1 - row_idx
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
    for y in range(MATRIX_HEIGHT + 1):
        ax.axhline(y, color="lightgray", linestyle="-", linewidth=0.5)

    # Draw vertical grid lines
    for x in range(MATRIX_WIDTH + 1):
        ax.axvline(x, color="lightgray", linestyle="-", linewidth=0.5)

# Adjust layout to prevent titles/labels from overlapping
plt.tight_layout()

# Display the plot
plt.show()
