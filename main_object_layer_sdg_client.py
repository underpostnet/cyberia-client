# Synthetic data generator client

import matplotlib.pyplot as plt
import numpy as np
import random
import math  # Import math for trigonometric functions
from object_layer.object_layer_sdg import (
    SyntheticDataGenerator,
    clarify_and_contrast_rgba,
)
from object_layer.object_layer_data_sdg import (
    SyntheticDataToolAPI,
)  # Import the new tool API

import argparse

# Import the default synthetic data template. This is the authoritative matrix.
from object_layer.object_layer_data import DEFAULT_PLAYER_SKIN_FRAME_DOWN_IDLE

# Convert the imported list to a NumPy array
DEFAULT_PLAYER_SKIN_FRAME_DOWN_IDLE = np.array(DEFAULT_PLAYER_SKIN_FRAME_DOWN_IDLE)


# Define the value mapping to RGBA (0-255) format for display
DISPLAY_COLOR_PALETTE = {
    0: (255, 255, 255, 255),  # White
    1: (224, 224, 224, 255),  # Gray light
    2: (255, 0, 0, 255),  # Red
    3: (0, 255, 0, 255),  # Green
    4: (0, 0, 255, 255),  # Blue
    5: (255, 255, 0, 255),  # Yellow
    6: (255, 0, 255, 255),  # Magenta
    7: (0, 255, 255, 255),  # Cyan
    8: (128, 0, 128, 255),  # Purple
    9: (255, 224, 189, 225),  # Light Skin
    10: (240, 192, 150, 225),  # Medium-Light Skin
    11: (218, 166, 126, 225),  # Medium Skin
    12: (186, 128, 92, 225),  # Medium-Dark Skin
    13: (139, 69, 19, 225),  # Dark Skin
    14: (0, 0, 0, 255),  # Hair black 0
    17: (150, 148, 0, 255),  # Hair yellow 0
    20: (104, 62, 0, 255),  # Hair brown 0
}

DISPLAY_COLOR_PALETTE[15] = clarify_and_contrast_rgba(
    DISPLAY_COLOR_PALETTE[14], 0.5, 1.3
)
DISPLAY_COLOR_PALETTE[16] = clarify_and_contrast_rgba(
    DISPLAY_COLOR_PALETTE[15], 0.5, 1.3
)

DISPLAY_COLOR_PALETTE[18] = clarify_and_contrast_rgba(
    DISPLAY_COLOR_PALETTE[17], 0.5, 1.3
)
DISPLAY_COLOR_PALETTE[19] = clarify_and_contrast_rgba(
    DISPLAY_COLOR_PALETTE[18], 0.5, 1.3
)

DISPLAY_COLOR_PALETTE[21] = clarify_and_contrast_rgba(
    DISPLAY_COLOR_PALETTE[20], 0.5, 1.3
)
DISPLAY_COLOR_PALETTE[22] = clarify_and_contrast_rgba(
    DISPLAY_COLOR_PALETTE[21], 0.5, 1.3
)

# Get the dimensions of the default matrix for boundary checks
DATA_MATRIX_HEIGHT, DATA_MATRIX_WIDTH = DEFAULT_PLAYER_SKIN_FRAME_DOWN_IDLE.shape


def render_factory(
    data_generator: SyntheticDataGenerator,
    tool_api: SyntheticDataToolAPI,
    mode: str = "skin-default",
):
    """
    Applies various rendering (data generation) operations based on the specified mode.

    Args:
        data_generator (SyntheticDataGenerator): The data generator instance.
        tool_api (SyntheticDataToolAPI): The tool API instance for complex operations.
        mode (str): The rendering mode.
    """
    if mode == "skin-default":
        # Adjust a specific color in the palette for this render mode
        DISPLAY_COLOR_PALETTE[1] = (0, 0, 0, 255)
        tool_api.apply_default_skin_template_fill(DISPLAY_COLOR_PALETTE)

    elif mode == "skin-default-0":
        # Define a focus region for data generation
        focus_region_x1, focus_region_y1 = 6, 23  # Bottom-left corner
        focus_region_x2, focus_region_y2 = 19, 18  # Top-right corner

        # Get all coordinates within this defined focus region
        seed_generation_positions = data_generator.get_coordinates_in_region(
            focus_region_x1, focus_region_y1, focus_region_x2, focus_region_y2
        )

        generation_curves = []
        generation_value_id = random.choice(
            list(range(2, 9))
        )  # Random color for curves

        for _ in range(20):
            generation_curves.append(
                {
                    "curve_value_id": generation_value_id,
                    "num_points": random.randint(50, 50),
                    "curve_type": random.choice(
                        [
                            "parabola",
                            "sigmoid",
                            "sine",
                            "linear",
                            "cubic",
                            "circle_arc",
                            "spiral",
                        ]
                    ),
                    "initial_x_pos": random.uniform(0, DATA_MATRIX_WIDTH),
                    "initial_y_pos": random.uniform(0, DATA_MATRIX_HEIGHT),
                }
            )

        for curve_config in generation_curves:
            initial_x_pos = curve_config["initial_x_pos"]
            initial_y_pos = curve_config["initial_y_pos"]

            # Optionally, start curves from a focus region
            if seed_generation_positions:
                chosen_seed_coord = random.choice(seed_generation_positions)
                initial_x_pos = chosen_seed_coord[0]
                initial_y_pos = chosen_seed_coord[1]

            tool_api.generate_complex_parametric_curve(
                curve_config["curve_type"],
                initial_x_pos,
                initial_y_pos,
                curve_config["curve_value_id"],
            )
        tool_api.apply_default_skin_template_fill(DISPLAY_COLOR_PALETTE)

    elif mode == "skin-default-1":
        render_color_hair = random.choice([14, 17, 20])

        for coord in data_generator.get_coordinates_in_region(7, 21, 10, 20):
            x, y = coord
            tool_api.generate_pattern_from_coordinates(
                x, y, render_color_hair, tool_api.create_coordinate_pattern("hair-lock")
            )
            tool_api.generate_pattern_from_coordinates(
                x,
                y - 1,
                render_color_hair + 1,
                tool_api.create_coordinate_pattern("hair-lock"),
            )
            tool_api.generate_pattern_from_coordinates(
                x,
                y - 2,
                render_color_hair + 2,
                tool_api.create_coordinate_pattern("hair-lock"),
            )

        for coord in data_generator.get_coordinates_in_region(8, 23, 14, 22):
            x, y = coord
            tool_api.generate_pattern_from_coordinates(
                x, y, render_color_hair, tool_api.create_coordinate_pattern("hair-lock")
            )
            tool_api.generate_pattern_from_coordinates(
                x,
                y - 1,
                render_color_hair + 1,
                tool_api.create_coordinate_pattern("hair-lock"),
            )
            tool_api.generate_pattern_from_coordinates(
                x,
                y - 2,
                render_color_hair + 2,
                tool_api.create_coordinate_pattern("hair-lock"),
            )

        for coord in data_generator.get_coordinates_in_region(11, 22, 15, 18):
            x, y = coord
            tool_api.generate_pattern_from_coordinates(
                x,
                y,
                render_color_hair,
                tool_api.create_coordinate_pattern("hair-lock"),
                lambda dx, dy: [dx * -1, dy],  # Apply a filter to mirror the pattern
            )
            tool_api.generate_pattern_from_coordinates(
                x,
                y - 1,
                render_color_hair + 1,
                tool_api.create_coordinate_pattern("hair-lock"),
            )
            tool_api.generate_pattern_from_coordinates(
                x,
                y - 2,
                render_color_hair + 2,
                tool_api.create_coordinate_pattern("hair-lock"),
            )

        tool_api.apply_default_skin_template_fill(DISPLAY_COLOR_PALETTE)


# --- Main Execution ---
if __name__ == "__main__":
    random.seed()  # Initialize random seed once at the start for true randomness across script runs

    parser = argparse.ArgumentParser(
        description="Generate synthetic data with parametric curves and focus regions."
    )

    parser.add_argument(
        "--mode",
        type=str,
        default="skin-default",
        help='Special mode for rendering. Current options: "skin-default", "skin-default-0", "skin-default-1".',
    )

    args = parser.parse_args()

    print(f"Running in mode: {args.mode}")

    # Create a figure and a 2x4 grid of subplots
    fig, axes = plt.subplots(2, 4, figsize=(26, 13), dpi=100)

    # Flatten the axes array for easy iteration
    axes = axes.flatten()

    # Configure each subplot to display the synthetic data on a 26x26 grid
    for i, ax in enumerate(axes):
        # Initialize a new SyntheticDataGenerator for each subplot with a fresh copy of the default matrix
        # This ensures each subplot starts with the original template and independent generations.
        data_generator = SyntheticDataGenerator(
            DEFAULT_PLAYER_SKIN_FRAME_DOWN_IDLE.copy(), DISPLAY_COLOR_PALETTE
        )
        # Initialize the SyntheticDataToolAPI for the current data generator
        tool_api = SyntheticDataToolAPI(data_generator)

        render_factory(data_generator, tool_api, args.mode)

        # Render the data matrix to the current subplot using the tool API
        tool_api.render_data_matrix_to_subplot(ax, i)

    # Adjust layout to prevent titles/labels from overlapping
    plt.tight_layout()

    # Display the plot
    plt.show()
