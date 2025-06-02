import matplotlib.pyplot as plt
import numpy as np
import random
import math  # Import math for trigonometric functions
from pixel_art_editor import PixelArtEditor  # Import the new class

import argparse

# Import the default player skin frame. This is the authoritative matrix.
from object_layer.object_layer_data import DEFAULT_PLAYER_SKIN_FRAME_DOWN_IDLE

# Convert the imported list to a NumPy array
DEFAULT_PLAYER_SKIN_FRAME_DOWN_IDLE = np.array(DEFAULT_PLAYER_SKIN_FRAME_DOWN_IDLE)


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
    8: (128, 0, 128, 255),  # Purple (R, G, B, Alpha)
    9: (255, 224, 189, 225),  # Light Skin
    10: (240, 192, 150, 225),  # Medium-Light Skin
    11: (218, 166, 126, 225),  # Medium Skin
    12: (186, 128, 92, 225),  # Medium-Dark Skin
    13: (139, 69, 19, 225),  # Dark Skin
}

# Get the dimensions of the default matrix for boundary checks
MATRIX_HEIGHT, MATRIX_WIDTH = DEFAULT_PLAYER_SKIN_FRAME_DOWN_IDLE.shape


# --- Define Parametric Curve Functions ---
# These functions return x and y functions for a given curve type,
# allowing for flexible positioning and scaling.
# These functions now return (x, y) coordinates where (0,0) is bottom-left.


def get_parabola_funcs(x_center, y_base, scale_x, scale_y, a):
    """
    Returns x and y functions for a parabola.
    x_center: horizontal position of the parabola's vertex.
    y_base: vertical position of the parabola's vertex.
    scale_x, scale_y: scaling factors for the curve's spread.
    a: controls the opening direction and width of the parabola.
    """
    x_func = lambda t: x_center + scale_x * t
    y_func = lambda t: y_base + scale_y * a * (t**2)
    return x_func, y_func


def get_sigmoid_funcs(x_center, y_midpoint, scale_x, scale_y, k, x0):
    """
    Returns x and y functions for a sigmoid curve.
    x_center: horizontal position of the sigmoid's center.
    y_midpoint: vertical position of the sigmoid's midpoint.
    scale_x, scale_y: scaling factors for the curve's spread.
    k: steepness of the curve.
    x0: x-value of the sigmoid's midpoint.
    """
    x_func = lambda t: x_center + scale_x * t
    y_func = lambda t: y_midpoint + scale_y * (1 / (1 + math.exp(-k * (t - x0))))
    return x_func, y_func


def get_sine_funcs(x_start, y_axis_offset, amplitude, frequency):
    """
    Returns x and y functions for a sine wave.
    x_start: horizontal starting position of the sine wave.
    y_axis_offset: vertical offset of the sine wave's central axis.
    amplitude: height of the wave.
    frequency: how many cycles in a given range.
    """
    x_func = lambda t: x_start + t
    y_func = lambda t: y_axis_offset + amplitude * math.sin(frequency * t)
    return x_func, y_func


def get_linear_funcs(x_start, y_start, x_end, y_end):
    """
    Returns x and y functions for a linear curve (straight line).
    x_start, y_start: coordinates of the line's starting point.
    x_end, y_end: coordinates of the line's ending point.
    """
    # Linear interpolation between two points using parameter t from 0 to 1
    x_func = lambda t: x_start + t * (x_end - x_start)
    y_func = lambda t: y_start + t * (y_end - y_start)
    return x_func, y_func


def get_cubic_funcs(x_center, y_center, scale_x, scale_y, a, b, c):
    """
    Returns x and y functions for a cubic curve.
    x_center, y_center: central position offsets for the cubic curve.
    scale_x, scale_y: scaling factors for the curve's spread.
    a, b, c: coefficients of the cubic polynomial.
    """
    x_func = lambda t: x_center + scale_x * t
    y_func = lambda t: y_center + scale_y * (a * (t**3) + b * (t**2) + c * t)
    return x_func, y_func


def get_circle_arc_funcs(center_x, center_y, radius, start_angle, end_angle):
    """
    Returns x and y functions for a circular arc.
    center_x, center_y: coordinates of the center of the circle.
    radius: radius of the arc.
    start_angle, end_angle: angular range for the arc in radians.
    """
    x_func = lambda t: center_x + radius * math.cos(t)
    y_func = lambda t: center_y + radius * math.sin(t)
    return x_func, y_func


def get_spiral_funcs(center_x, center_y, radius_growth_rate, angular_speed):
    """
    Returns x and y functions for a spiral curve.
    center_x, center_y: coordinates of the spiral's origin.
    radius_growth_rate: how quickly the spiral expands.
    angular_speed: how quickly the spiral rotates.
    """
    x_func = lambda t: center_x + (radius_growth_rate * t) * math.cos(angular_speed * t)
    y_func = lambda t: center_y + (radius_growth_rate * t) * math.sin(angular_speed * t)
    return x_func, y_func


# --- Main Execution ---
if __name__ == "__main__":
    random.seed()  # Initialize random seed once at the start for true randomness across script runs

    parser = argparse.ArgumentParser(
        description="Generate pixel art with parametric curves and seed positions."
    )

    parser.add_argument(
        "--mode",
        type=str,
        default="skin-default",
        help='Special mode for rendering. Current options: "skin-default", "skin-default-0", "skin-default-1", "skin-default-2".',
    )

    args = parser.parse_args()

    print(f"Running in mode: {args.mode}")

    # Create a figure and a 2x4 grid of subplots
    fig, axes = plt.subplots(2, 4, figsize=(26, 13), dpi=100)

    # Flatten the axes array for easy iteration
    axes = axes.flatten()

    # Configure each subplot to display the pixel art on a 26x26 grid
    for i, ax in enumerate(axes):
        # Initialize a new PixelArtEditor for each subplot with a fresh copy of the default matrix
        # This ensures each subplot starts with the original silhouette and independent drawings.
        editor = PixelArtEditor(
            DEFAULT_PLAYER_SKIN_FRAME_DOWN_IDLE.copy(), COLOR_PALETTE
        )
        if args.mode == "skin-default":

            # --- Demonstrate Drawing by directly overwriting pixels ---
            # Example drawing operation: Draw a red pixel on the silhouette (border)
            # The draw_pixel now expects (x, y) where (0,0) is bottom-left
            # editor.draw_pixel(0, 0, 2)  # Draw red (color ID 2) at (0,0) - bottom-left
            # editor.draw_pixel(1, 0, 2)  # Draw red (color ID 2) at (1,0)
            # editor.draw_pixel(0, 1, 2)  # Draw red (color ID 2) at (0,1)
            # editor.draw_pixel(1, 1, 2)  # Draw red (color ID 2) at (1,1)

            # Add your requested green pixel draws *after* the previous operations
            # editor.draw_pixel(11, 11, 3)  # Draw green (color ID 3)
            # editor.draw_pixel(12, 12, 3)  # Draw green (color ID 3)
            # editor.draw_pixel(13, 13, 3)  # Draw green (color ID 3)

            # --- Add 2 random rectangles ---
            for _ in range(0):  # Set to 0 for now as per original code
                # Position variables for rectangle (using x, y for bottom-left)
                rect_start_x = random.randint(0, MATRIX_WIDTH - 5)
                rect_start_y = random.randint(0, MATRIX_HEIGHT - 5)
                rect_width = random.randint(3, 8)
                rect_height = random.randint(3, 8)
                rect_color = random.choice(
                    [2, 3, 4, 5, 6, 7]
                )  # Random color excluding white/black
                editor.draw_rectangle(
                    rect_start_x, rect_start_y, rect_width, rect_height, rect_color
                )

            # --- Define and Render "Draw Areas" (e.g., for draw positions) ---
            # This area will be highlighted with a specific color (e.g., Purple, color ID 8)
            # The area is defined by two (x,y) coordinates.
            # For simulating "AI up", we define a region where draw might be.
            draw_area_x1, draw_area_y1 = 6, 23  # Bottom-left corner of the draw area
            draw_area_x2, draw_area_y2 = 19, 18  # Top-right corner of the draw area

            # Get all coordinates within this defined "bad area"
            seed_draw_positions = editor.get_coordinates_in_area(
                draw_area_x1, draw_area_y1, draw_area_x2, draw_area_y2
            )

            # Draw the "bad area" with a distinct color (Purple, color ID 8)
            # This visualizes the region where the AI might focus its "draw" drawing.
            # for x_coord, y_coord in seed_draw_positions:
            #     editor.draw_pixel(x_coord, y_coord, 8)  # Using color ID 8 for purple

            draw_curves = []
            draw_hair_color = random.choice([2, 3, 4, 5, 6, 7])

            for _ in range(20):
                draw_curves.append(
                    {
                        "curve_color": draw_hair_color,  # Random color
                        "num_points": random.randint(50, 50),
                        "curve_type": "parabola",
                        "initial_x_pos": random.uniform(0, MATRIX_WIDTH),
                        "initial_y_pos": random.uniform(0, MATRIX_HEIGHT),
                    }
                )

            for curve_num in range(len(draw_curves)):
                curve_color = draw_curves[curve_num]["curve_color"]
                num_points = draw_curves[curve_num]["num_points"]
                curve_type = draw_curves[curve_num]["curve_type"]

                # For the first curve, try to start it from a "seed draw position"
                if True or curve_num == 0 and seed_draw_positions:
                    # Randomly select a coordinate from the "bad area" for the curve's starting point/center
                    chosen_seed_coord = random.choice(seed_draw_positions)
                    # seed_draw_positions returns (x, y)
                    initial_x_pos = chosen_seed_coord[0]
                    initial_y_pos = chosen_seed_coord[1]
                else:
                    # For other curves, or if no seed positions, use random positions
                    initial_x_pos = draw_curves[curve_num]["initial_x_pos"]
                    initial_y_pos = draw_curves[curve_num]["initial_y_pos"]

                if curve_type == "parabola":
                    # Position variables for parabola
                    parabola_center_x = initial_x_pos
                    parabola_base_y = initial_y_pos
                    scale_x = random.uniform(5, 15)
                    scale_y = random.uniform(5, 15)
                    a = random.uniform(
                        -0.5, 0.5
                    )  # Controls opening direction and width
                    x_func, y_func = get_parabola_funcs(
                        parabola_center_x, parabola_base_y, scale_x, scale_y, a
                    )
                    t_start = -1.0
                    t_end = 1.0

                elif curve_type == "sigmoid":
                    # Position variables for sigmoid
                    sigmoid_center_x = initial_x_pos
                    sigmoid_midpoint_y = initial_y_pos
                    scale_x = random.uniform(5, 15)
                    scale_y = random.uniform(5, 15)
                    k = random.uniform(0.5, 5.0)  # Steepness
                    x0 = random.uniform(-0.5, 0.5)  # x-value of midpoint
                    x_func, y_func = get_sigmoid_funcs(
                        sigmoid_center_x, sigmoid_midpoint_y, scale_x, scale_y, k, x0
                    )
                    t_start = -5.0
                    t_end = 5.0

                elif curve_type == "sine":
                    # Position variables for sine wave
                    sine_start_x = initial_x_pos
                    sine_y_axis_offset = initial_y_pos
                    amplitude = random.uniform(3, 10)
                    frequency = random.uniform(0.5, 3.0)
                    x_func, y_func = get_sine_funcs(
                        sine_start_x, sine_y_axis_offset, amplitude, frequency
                    )
                    t_start = 0
                    t_end = random.uniform(
                        MATRIX_WIDTH / 2, MATRIX_WIDTH
                    )  # Spread horizontally

                elif curve_type == "linear":
                    # Position variables for linear curve
                    line_start_x = initial_x_pos
                    line_start_y = initial_y_pos
                    line_end_x = random.uniform(0, MATRIX_WIDTH)
                    line_end_y = random.uniform(0, MATRIX_HEIGHT)
                    x_func, y_func = get_linear_funcs(
                        line_start_x, line_start_y, line_end_x, line_end_y
                    )
                    t_start = 0.0
                    t_end = 1.0  # t goes from 0 to 1 for linear interpolation

                elif curve_type == "cubic":
                    # Position variables for cubic curve
                    cubic_center_x = initial_x_pos
                    cubic_center_y = initial_y_pos
                    scale_x = random.uniform(5, 15)
                    scale_y = random.uniform(5, 15)
                    a = random.uniform(-0.1, 0.1)  # Coefficients for curve shape
                    b = random.uniform(-0.5, 0.5)
                    c = random.uniform(-0.5, 0.5)
                    x_func, y_func = get_cubic_funcs(
                        cubic_center_x, cubic_center_y, scale_x, scale_y, a, b, c
                    )
                    t_start = -1.0
                    t_end = 1.0

                elif curve_type == "circle_arc":
                    # Position variables for circle arc
                    arc_center_x = initial_x_pos
                    arc_center_y = initial_y_pos
                    radius = random.uniform(5, min(MATRIX_WIDTH, MATRIX_HEIGHT) / 2 - 2)
                    start_angle = random.uniform(0, 2 * math.pi)
                    end_angle = start_angle + random.uniform(
                        math.pi / 4, 1.5 * math.pi
                    )  # Arc length
                    x_func, y_func = get_circle_arc_funcs(
                        arc_center_x, arc_center_y, radius, start_angle, end_angle
                    )
                    t_start = start_angle
                    t_end = end_angle

                elif curve_type == "spiral":
                    # Position variables for spiral
                    spiral_center_x = initial_x_pos
                    spiral_center_y = initial_y_pos
                    radius_growth_rate = random.uniform(0.5, 2.0)
                    angular_speed = random.uniform(1.0, 3.0)
                    x_func = lambda t: spiral_center_x + (
                        radius_growth_rate * t
                    ) * math.cos(angular_speed * t)
                    y_func = lambda t: spiral_center_y + (
                        radius_growth_rate * t
                    ) * math.sin(angular_speed * t)
                    t_start = 0
                    t_end = random.uniform(
                        2 * math.pi, 6 * math.pi
                    )  # Vary the length of the curve

                editor.draw_parametric_curve(
                    x_func, y_func, t_start, t_end, num_points, curve_color
                )

            # --- Demonstrate the new flood_fill method ---
            # Apply flood fill only to the first graph for clear demonstration
            skin_color_id = random.choice(list(range(9, 14)))

            editor.flood_fill(12, 12, fill_color_id=skin_color_id)
            editor.flood_fill(7, 4, fill_color_id=skin_color_id)
            editor.flood_fill(18, 4, fill_color_id=skin_color_id)

            editor.flood_fill(13, 7, fill_color_id=random.choice(list(range(2, 9))))

            editor.flood_fill(12, 4, fill_color_id=random.choice(list(range(2, 9))))

            shoes_color_id = random.choice(list(range(2, 9)))
            editor.flood_fill(9, 2, fill_color_id=shoes_color_id)
            editor.flood_fill(15, 2, fill_color_id=shoes_color_id)
        elif args.mode == "skin-default-0":
            editor.draw_pixel(0, 0, 2)  # Draw red (color ID 2) at (0,0) - bottom-left

        # Set subplot limits and labels
        ax.set_xlim(0, MATRIX_WIDTH)
        ax.set_ylim(0, MATRIX_HEIGHT)
        ax.set_xlabel("Pixel X")
        ax.set_ylabel("Pixel Y")
        ax.set_aspect("equal", adjustable="box")
        ax.set_title(f"Pixel Art {i+1}")  # Add a title for each subplot

        # Iterate over the matrix to draw each pixel as a colored square
        # The matrix now stores data as if (0,0) is top-left, but we plot it
        # as if (0,0) is bottom-left. So, the plotting logic needs to invert Y.
        for row_idx in range(MATRIX_HEIGHT):
            for col_idx in range(MATRIX_WIDTH):
                pixel_value = editor.matrix[row_idx, col_idx]
                color = editor.get_mpl_color(pixel_value)

                # Calculate the coordinates for the current pixel's square
                # Invert y-coordinate for drawing to match matrix (0 at top) to plot (0 at bottom)
                # This is the crucial part that aligns the matrix data with Matplotlib's y-axis.
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
