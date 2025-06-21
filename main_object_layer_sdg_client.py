# Synthetic data generator client

import json
from typing import Union
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
from object_layer.object_layer_data import (
    DEFAULT_PLAYER_SKIN_FRAME_DOWN_IDLE,
    DEFAULT_PLAYER_SKIN_FRAME_RIGHT_IDLE,
)

# Convert the imported list to a NumPy array
DEFAULT_PLAYER_SKIN_FRAME_DOWN_IDLE = np.array(DEFAULT_PLAYER_SKIN_FRAME_DOWN_IDLE)


# Define the value mapping to RGBA (0-255) format for display
DISPLAY_COLOR_PALETTE = {
    0: (0, 0, 0, 0),  # Transparent
    1: (0, 0, 0, 255),  # Black
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


class SkinColorProfile:
    """Manages loading, saving, and providing skin feature colors."""

    SKIN = "skin_color_id"
    HAIR_BASE = "hair_base_color_id"
    EYE = "eye_color_id"
    SHOES = "shoes_color_id"
    SHIRT = "shirt_color_id"  # For region (13,7)
    PANTS = "pants_color_id"  # For region (12,4)

    ALL_FEATURE_KEYS = [SKIN, HAIR_BASE, EYE, SHOES, SHIRT, PANTS]

    def __init__(self):
        self.colors = {key: None for key in self.ALL_FEATURE_KEYS}

    def load_from_dict(self, color_data: dict):
        """Populates colors from a dictionary."""
        if not isinstance(color_data, dict):
            print(
                f"Warning: Invalid data provided for loading profile. Expected dict, got {type(color_data)}."
            )
            return
        for key in self.ALL_FEATURE_KEYS:
            if key in color_data:
                self.colors[key] = color_data[key]

    def to_dict(self) -> dict:
        """Ensures all colors are populated and returns the color dictionary."""
        self.ensure_all_colors_populated()
        return self.colors

    def get_skin_color(self) -> int:
        if self.colors[self.SKIN] is None:
            self.colors[self.SKIN] = random.choice(list(range(9, 14)))
        return self.colors[self.SKIN]

    def get_hair_base_color(self) -> int:
        if self.colors[self.HAIR_BASE] is None:
            self.colors[self.HAIR_BASE] = random.choice([14, 17, 20])
        return self.colors[self.HAIR_BASE]

    def get_eye_color(self) -> int:
        if self.colors[self.EYE] is None:
            if random.random() < 0.5:  # random.random() gives float in [0.0, 1.0)
                self.colors[self.EYE] = 14
            else:
                self.colors[self.EYE] = random.choice([3, 4, 20])
        return self.colors[self.EYE]

    def get_shoes_color(self) -> int:
        if self.colors[self.SHOES] is None:
            self.colors[self.SHOES] = random.choice(list(range(2, 9)))
        return self.colors[self.SHOES]

    def get_shirt_color(self) -> int:  # For region (13,7)
        if self.colors[self.SHIRT] is None:
            self.colors[self.SHIRT] = random.choice(list(range(2, 9)))
        return self.colors[self.SHIRT]

    def get_pants_color(self) -> int:  # For region (12,4)
        if self.colors[self.PANTS] is None:
            self.colors[self.PANTS] = random.choice(list(range(2, 9)))
        return self.colors[self.PANTS]

    def ensure_all_colors_populated(self):
        """Ensures all feature colors have a value, generating if necessary."""
        for key_method_name in [
            "get_skin_color",
            "get_hair_base_color",
            "get_eye_color",
            "get_shoes_color",
            "get_shirt_color",
            "get_pants_color",
        ]:
            getattr(self, key_method_name)()


def render_factory(
    data_generator: SyntheticDataGenerator,
    tool_api: SyntheticDataToolAPI,
    mode: str = "skin-default",
    skin_color_profile: Union[SkinColorProfile, None] = None,
):
    """
    Applies various rendering (data generation) operations based on the specified mode.

    Args:
        data_generator (SyntheticDataGenerator): The data generator instance.
        tool_api (SyntheticDataToolAPI): The tool API instance for complex operations.
        mode (str): The rendering mode.
        skin_color_profile (SkinColorProfile): Profile for managing skin feature colors.
    """
    DISPLAY_COLOR_PALETTE[1] = (0, 0, 0, 255)
    # The data_generator will handle making this palette compact and unique internally.

    if skin_color_profile is None:  # Should not happen if called from main
        skin_color_profile = SkinColorProfile()

    if mode == "skin-default":
        tool_api.apply_default_skin_template_fill(
            mode=mode,
            skin_color_semantic_id=skin_color_profile.get_skin_color(),
            shoes_color_semantic_id=skin_color_profile.get_shoes_color(),
            shirt_color_semantic_id=skin_color_profile.get_shirt_color(),
            pants_color_semantic_id=skin_color_profile.get_pants_color(),
        )

    elif mode == "skin-default-0":
        # Define a focus region for data generation
        focus_region_x1, focus_region_y1 = 6, 23  # Bottom-left corner
        focus_region_x2, focus_region_y2 = 19, 18  # Top-right corner

        # Get all coordinates within this defined focus region
        seed_generation_positions = data_generator.get_coordinates_in_region(
            focus_region_x1, focus_region_y1, focus_region_x2, focus_region_y2
        )

        generation_curves = []
        # Use semantic value IDs for generation, ToolAPI will convert
        generation_semantic_value_id = random.choice(
            list(range(2, 9))
        )  # Random color for curves

        for _ in range(3):
            curve_type = random.choice(
                [
                    "parabola",
                    "sigmoid",
                    "sine",
                    "linear",
                    "cubic",
                    "circle_arc",
                    "spiral",
                ]
            )

            # Default parameters for less messy curves
            num_points = 50
            initial_x_pos = random.uniform(5, DATA_MATRIX_WIDTH - 5)
            initial_y_pos = random.uniform(5, DATA_MATRIX_HEIGHT - 5)

            # Adjust parameters based on curve type for better visual appeal
            if curve_type == "parabola":
                scale_x = random.uniform(2, 8)
                scale_y = random.uniform(2, 8)
                a = random.uniform(-0.2, 0.2)
            elif curve_type == "sigmoid":
                scale_x = random.uniform(2, 8)
                scale_y = random.uniform(2, 8)
                k = random.uniform(0.8, 3.0)
                x0 = random.uniform(-0.2, 0.2)
            elif curve_type == "sine":
                amplitude = random.uniform(2, 6)
                frequency = random.uniform(0.2, 1.0)
                initial_x_pos = random.uniform(
                    0, DATA_MATRIX_WIDTH - 10
                )  # Start further left
                initial_y_pos = random.uniform(5, DATA_MATRIX_HEIGHT - 5)
            elif curve_type == "linear":
                # Linear curves are inherently less "messy"
                pass
            elif curve_type == "cubic":
                scale_x = random.uniform(2, 8)
                scale_y = random.uniform(2, 8)
                a = random.uniform(-0.05, 0.05)
                b = random.uniform(-0.2, 0.2)
                c = random.uniform(-0.2, 0.2)
            elif curve_type == "circle_arc":
                radius = random.uniform(
                    5, min(DATA_MATRIX_WIDTH, DATA_MATRIX_HEIGHT) / 4
                )
                start_angle = random.uniform(0, 2 * math.pi)
                end_angle = start_angle + random.uniform(
                    math.pi / 6, math.pi
                )  # Smaller arcs
            elif curve_type == "spiral":
                radius_growth_rate = random.uniform(0.2, 1.0)
                angular_speed = random.uniform(0.5, 2.0)
                num_points = 100  # More points for smoother spiral
                initial_x_pos = DATA_MATRIX_WIDTH / 2
                initial_y_pos = DATA_MATRIX_HEIGHT / 2

            curve_config = {
                "curve_semantic_value_id": generation_semantic_value_id,
                "num_points": num_points,
                "curve_type": curve_type,
                "initial_x_pos": initial_x_pos,
                "initial_y_pos": initial_y_pos,
                # Add specific parameters for each curve type here if needed
                "parabola_params": (
                    {"scale_x": scale_x, "scale_y": scale_y, "a": a}
                    if curve_type == "parabola"
                    else None
                ),
                "sigmoid_params": (
                    {"scale_x": scale_x, "scale_y": scale_y, "k": k, "x0": x0}
                    if curve_type == "sigmoid"
                    else None
                ),
                "sine_params": (
                    {"amplitude": amplitude, "frequency": frequency}
                    if curve_type == "sine"
                    else None
                ),
                "cubic_params": (
                    {"scale_x": scale_x, "scale_y": scale_y, "a": a, "b": b, "c": c}
                    if curve_type == "cubic"
                    else None
                ),
                "circle_arc_params": (
                    {
                        "radius": radius,
                        "start_angle": start_angle,
                        "end_angle": end_angle,
                    }
                    if curve_type == "circle_arc"
                    else None
                ),
                "spiral_params": (
                    {
                        "radius_growth_rate": radius_growth_rate,
                        "angular_speed": angular_speed,
                    }
                    if curve_type == "spiral"
                    else None
                ),
            }
            generation_curves.append(curve_config)

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
                curve_config["curve_semantic_value_id"],  # Pass semantic ID
            )
        tool_api.apply_default_skin_template_fill(
            mode=mode,
            skin_color_semantic_id=skin_color_profile.get_skin_color(),
            shoes_color_semantic_id=skin_color_profile.get_shoes_color(),
            shirt_color_semantic_id=skin_color_profile.get_shirt_color(),
            pants_color_semantic_id=skin_color_profile.get_pants_color(),
        )

    elif mode in [
        "skin-default-08-0",
        "skin-default-08-1",
        "skin-default-18-0",
        "skin-default-18-1",
        "skin-default-06-0",
        "skin-default-06-1",
        "skin-default-16-0",
        "skin-default-16-1",
        "skin-default-04-0",
        "skin-default-04-1",
        "skin-default-14-0",
        "skin-default-14-1",
        "skin-default-02-0",
        "skin-default-02-1",
        "skin-default-12-0",
        "skin-default-12-1",
    ]:
        render_color_hair_semantic = skin_color_profile.get_hair_base_color()

        if mode in [
            "skin-default-02-0",
            "skin-default-02-1",
            "skin-default-12-0",
            "skin-default-12-1",
        ]:
            tool_api.data_generator.generate_rectangular_region(
                7, 9, abs(18 - 7), abs(18 - 9), render_color_hair_semantic
            )

        for coord in data_generator.get_coordinates_in_region(7, 21, 10, 20):
            x, y = coord
            tool_api.generate_pattern_from_coordinates(
                x,
                y,
                render_color_hair_semantic,
                tool_api.create_coordinate_pattern("hair-lock"),
            )
            tool_api.generate_pattern_from_coordinates(
                x,
                y - 1,
                render_color_hair_semantic
                + 1,  # Assuming semantic IDs for shades are contiguous
                tool_api.create_coordinate_pattern("hair-lock"),
            )
            tool_api.generate_pattern_from_coordinates(
                x,
                y - 2,
                render_color_hair_semantic + 2,
                tool_api.create_coordinate_pattern("hair-lock"),
            )

        for coord in data_generator.get_coordinates_in_region(8, 23, 14, 22):
            x, y = coord
            tool_api.generate_pattern_from_coordinates(
                x,
                y,
                render_color_hair_semantic,
                tool_api.create_coordinate_pattern("hair-lock"),
            )
            tool_api.generate_pattern_from_coordinates(
                x,
                y - 1,
                render_color_hair_semantic + 1,
                tool_api.create_coordinate_pattern("hair-lock"),
            )
            tool_api.generate_pattern_from_coordinates(
                x,
                y - 2,
                render_color_hair_semantic + 2,
                tool_api.create_coordinate_pattern("hair-lock"),
            )

        for coord in data_generator.get_coordinates_in_region(11, 22, 15, 18):
            x, y = coord
            tool_api.generate_pattern_from_coordinates(
                x,
                y,
                render_color_hair_semantic,
                tool_api.create_coordinate_pattern("hair-lock"),
                lambda dx, dy: [dx * -1, dy],  # Apply a filter to mirror the pattern
            )
            tool_api.generate_pattern_from_coordinates(
                x,
                y - 1,
                render_color_hair_semantic + 1,
                tool_api.create_coordinate_pattern("hair-lock"),
            )
            tool_api.generate_pattern_from_coordinates(
                x,
                y - 2,
                render_color_hair_semantic + 2,
                tool_api.create_coordinate_pattern("hair-lock"),
            )

        tool_api.apply_default_skin_template_fill(
            mode=mode,
            skin_color_semantic_id=skin_color_profile.get_skin_color(),
            shoes_color_semantic_id=skin_color_profile.get_shoes_color(),
            shirt_color_semantic_id=skin_color_profile.get_shirt_color(),
            pants_color_semantic_id=skin_color_profile.get_pants_color(),
        )

        eye_color_semantic = skin_color_profile.get_eye_color()
        # Eye placement logic remains the same, only color source changes
        if mode in [
            "skin-default-04-0",
            "skin-default-04-1",
            "skin-default-14-0",
            "skin-default-14-1",
        ]:
            data_generator.set_data_point(
                abs(tool_api.data_matrix_width - 1 - 9), 13, eye_color_semantic
            )
        elif mode in [
            "skin-default-06-0",
            "skin-default-06-1",
            "skin-default-16-0",
            "skin-default-16-1",
        ]:
            data_generator.set_data_point(
                abs(tool_api.data_matrix_width - 1 - 9), 13, eye_color_semantic
            )
        elif mode in [
            "skin-default-08-0",
            "skin-default-08-1",
            "skin-default-18-0",
            "skin-default-18-1",
        ]:

            data_generator.set_data_point(10, 13, eye_color_semantic)
            data_generator.set_data_point(15, 13, eye_color_semantic)

        if mode in [
            "skin-default-04-0",
            "skin-default-04-1",
            "skin-default-14-0",
            "skin-default-14-1",
        ]:
            tool_api.flip_all_rows_horizontal()

        if mode in [
            "skin-default-12-0",
            "skin-default-14-0",
            "skin-default-16-0",
            "skin-default-18-0",
        ]:
            tool_api.cut_region(8, 3, 11, 1)
            tool_api.paste_region(8, 4)
        if mode in [
            "skin-default-12-1",
            "skin-default-14-1",
            "skin-default-16-1",
            "skin-default-18-1",
        ]:
            tool_api.cut_region(14, 3, 17, 1)
            tool_api.paste_region(14, 4)
    elif mode == "gfx-shadow-ball":
        canvas_size = 15
        tool_api.create_empty_canvas(
            canvas_size, fill_semantic_value_id=0  # semantic ID 0 for white
        )  # Create a 15x15 white canvas

        # Define colors for the two circles
        color_ball_1_semantic = 2  # Red (semantic ID)
        color_ball_2_semantic = 4  # Blue (semantic ID)

        # Draw first circle
        center_x_1, center_y_1, radius_1 = 4, 4, 3
        tool_api.draw_circle(center_x_1, center_y_1, radius_1, color_ball_1_semantic)
        # Apply shadow gradient to the first circle
        data_generator.contiguous_region_fill(
            center_x_1,
            center_y_1,
            fill_semantic_value_id=color_ball_1_semantic,
            gradient_shadow=True,
            intensity_factor=0.6,
            direction=random.choice(
                ["left_to_right", "right_to_left", "top_to_bottom", "bottom_to_top"]
            ),
        )

        # Draw second circle
        center_x_2, center_y_2, radius_2 = 10, 10, 4
        tool_api.draw_circle(center_x_2, center_y_2, radius_2, color_ball_2_semantic)
        # Apply shadow gradient to the second circle
        data_generator.contiguous_region_fill(
            center_x_2,
            center_y_2,
            fill_semantic_value_id=color_ball_2_semantic,
            gradient_shadow=True,
            intensity_factor=0.7,
            direction=random.choice(
                ["left_to_right", "right_to_left", "top_to_bottom", "bottom_to_top"]
            ),
        )

    elif mode == "skin-fluff":
        canvas_size = 20
        tool_api.create_empty_canvas(
            canvas_size, fill_semantic_value_id=0  # semantic ID 0 for white
        )  # Create a 20x20 white canvas

        # Body
        body_color_semantic = random.choice(
            list(range(2, 9))
        )  # Random vibrant color (semantic ID)
        data_generator.generate_rectangular_region(
            5, 5, 10, 10, body_color_semantic
        )  # Corrected call
        data_generator.contiguous_region_fill(
            7,
            7,
            fill_semantic_value_id=body_color_semantic,
            gradient_shadow=True,
            intensity_factor=0.4,
            direction=random.choice(["top_to_bottom", "bottom_to_top"]),
        )

        # Eyes (box eyes)
        eye_color = 14  # Black
        data_generator.generate_rectangular_region(7, 12, 2, 2, eye_color)
        data_generator.generate_rectangular_region(11, 12, 2, 2, eye_color)

        # Mouth
        mouth_color = 2  # Red
        data_generator.generate_rectangular_region(9, 9, 2, 1, mouth_color)

        # Feet/Base
        feet_color_semantic = random.choice(list(range(2, 9)))
        data_generator.generate_rectangular_region(6, 3, 3, 2, feet_color_semantic)
        data_generator.generate_rectangular_region(11, 3, 3, 2, feet_color_semantic)

        # Antenna
        antenna_color_semantic = random.choice(list(range(2, 9)))
        data_generator.generate_rectangular_region(9, 15, 2, 3, antenna_color_semantic)
        tool_api.draw_circle(10, 19, 1, antenna_color_semantic)  # Top of antenna

        # Random spots/details
        for _ in range(random.randint(2, 5)):
            spot_x = random.randint(6, 13)
            spot_y = random.randint(6, 13)
            spot_color_semantic = random.choice(list(range(2, 9)))
            data_generator.generate_rectangular_region(
                spot_x, spot_y, 1, 1, spot_color_semantic
            )

    elif mode == "cut-paste-demo":
        canvas_size = 20
        tool_api.create_empty_canvas(
            canvas_size, fill_semantic_value_id=0  # semantic ID 0 for white
        )  # Create a 20x20 white canvas

        # 1. Draw something to cut
        rect_color_semantic = 2  # Red (semantic ID)
        # Use data_generator directly for generate_rectangular_region as it's part of SDG core
        data_generator.generate_rectangular_region(
            start_x=2,
            start_y=2,
            width=5,
            height=5,
            semantic_value_id=rect_color_semantic,
        )

        circle_color_semantic = 4  # Blue (semantic ID)
        tool_api.draw_circle(
            center_x=10, center_y=10, radius=3, semantic_value_id=circle_color_semantic
        )

        # 2. Define cut region for the rectangle and cut it
        # Rectangle is from (2,2) to (6,6). Let's cut a 3x3 part from its center.
        cut_rect_x1, cut_rect_y1 = 3, 3
        cut_rect_x2, cut_rect_y2 = 5, 5
        print(
            f"Cutting rectangle part from ({cut_rect_x1},{cut_rect_y1}) to ({cut_rect_x2},{cut_rect_y2})"
        )
        tool_api.cut_region(
            cut_rect_x1,
            cut_rect_y1,
            cut_rect_x2,
            cut_rect_y2,
            clear_semantic_value_id=0,  # semantic ID 0 for white
        )  # Clear with white

        # 3. Define paste location and paste the cut rectangle part
        # Original bottom-left was (12,3). Cut region is 3x3.
        # New top-left Y = old_bottom_left_y + height - 1 = 3 + 3 - 1 = 5.
        # New top-left X = old_bottom_left_x = 12.
        paste_rect_top_left_x, paste_rect_top_left_y = 12, 5
        print(
            f"Pasting rectangle part with top-left at ({paste_rect_top_left_x},{paste_rect_top_left_y})"
        )
        tool_api.paste_region(paste_rect_top_left_x, paste_rect_top_left_y)

        # 4. Now, cut the entire circle and paste it
        # Circle center (10,10), radius 3. Bounding box: (7,7) to (13,13)
        # This is a 7x7 region.
        print(f"Cutting circle from (7,7) to (13,13)")
        tool_api.cut_region(
            x1=7,
            y1=7,
            x2=13,
            y2=13,
            clear_semantic_value_id=3,  # semantic ID 3 for Green
        )  # Clear with Green (value 3 is Green)

        # Adjust paste location so the 7-pixel high circle fits on the 20x20 canvas (max Y index 19)
        # e.g., paste_y_start_user = 19 - 7 + 1 = 13 for top edge alignment. Let's use 12.
        # Original bottom-left was (2,13). Circle region is 7x7.
        # New top-left Y = old_bottom_left_y + height - 1 = 13 + 7 - 1 = 19.
        # New top-left X = old_bottom_left_x = 2.
        paste_circle_top_left_x, paste_circle_top_left_y = 2, 19
        print(
            f"Pasting circle with top-left at ({paste_circle_top_left_x},{paste_circle_top_left_y})"
        )
        tool_api.paste_region(paste_circle_top_left_x, paste_circle_top_left_y)

    elif mode == "flip-demo":
        canvas_width = 10
        canvas_height = 5
        tool_api.create_empty_canvas(
            canvas_height,  # This creates a square canvas.
            fill_semantic_value_id=0,
        )
        # For non-square, let's re-initialize data_matrix directly for simplicity in demo
        data_generator.data_matrix = np.full(
            (canvas_height, canvas_width), 0, dtype=int
        )
        tool_api.data_matrix_height, tool_api.data_matrix_width = (
            data_generator.data_matrix.shape
        )

        # Draw distinct patterns on a few rows
        # These are semantic IDs
        # Row at user_y = 1: Red (2) on left, Blue (4) on right
        for x in range(canvas_width // 2):
            data_generator.set_data_point(x, 1, 2)  # Red (semantic)
        for x in range(canvas_width // 2, canvas_width):
            data_generator.set_data_point(x, 1, 4)  # Blue (semantic)

        # Row at user_y = 3: Green (3) on left, Yellow (5) on right
        for x in range(canvas_width // 2):
            data_generator.set_data_point(x, 3, 3)  # Green (semantic)
        for x in range(canvas_width // 2, canvas_width):
            data_generator.set_data_point(x, 3, 5)  # Yellow (semantic)

        # Row at user_y = 2: All Magenta (6)
        for x in range(canvas_width):
            data_generator.set_data_point(x, 2, 6)  # Magenta (semantic)

        # --- Step 1: Show initial state (implicitly done by rendering at the end) ---
        # --- Step 2: Flip a specific row ---
        row_to_flip_y_user = 1  # The Red/Blue row
        # To make the demo clear, we'll only do one type of flip per run for some subplots.
        # This example will flip row 1, then all rows.

        # print(f"Demo: Flipping row at user y={row_to_flip_y_user} horizontally.")
        # tool_api.flip_specific_row_horizontal(row_y_user=row_to_flip_y_user)

        # --- Step 3: Flip all rows (will also re-flip the already flipped row) ---
        # print(f"Demo: Flipping all rows horizontally.")
        tool_api.flip_all_rows_horizontal()  # Uncomment this line to see all rows flipped after the specific row flip


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
        help='Special mode for rendering. Current options: "skin-default", ..., "cut-paste-demo", "flip-demo".',
    )
    parser.add_argument(
        "--save-skin-colors",
        type=str,
        default=None,
        help="Filepath to save the current skin feature colors as a JSON file.",
    )
    parser.add_argument(
        "--load-skin-colors",
        type=str,
        default=None,
        help="Filepath to load skin feature colors from a JSON file.",
    )
    parser.add_argument(
        "--show",
        action="store_true",  # Sets args.show to True if present, False otherwise
        help="Display the generated graphics. If not provided, graphics will be generated but not shown.",
    )
    parser.add_argument(
        "--save-graph-data",
        type=str,
        default=None,
        help="Save individual graph data as JSON files using the provided string as a prefix (e.g., <prefix>_0.json). "
        "Each file contains 'FRAME' (data matrix) and 'COLOR' (compacted RGBA list).",
    )

    args = parser.parse_args()

    print(f"Running in mode: {args.mode}")

    # Determine the number of subplots based on the layout
    # For a 2x4 grid, it's 8.
    NUM_SUBPLOTS = 8
    skin_color_profiles = [SkinColorProfile() for _ in range(NUM_SUBPLOTS)]

    if args.load_skin_colors:
        try:
            with open(args.load_skin_colors, "r") as f:
                loaded_profiles_data = json.load(f)
            if isinstance(loaded_profiles_data, list):
                for i, profile_data in enumerate(loaded_profiles_data):
                    if i < NUM_SUBPLOTS:
                        if isinstance(profile_data, dict):
                            skin_color_profiles[i].load_from_dict(profile_data)
                        else:
                            print(
                                f"Warning: Item at index {i} in '{args.load_skin_colors}' is not a dictionary. Using default for this profile."
                            )
                    else:
                        break  # Loaded more profiles than subplots
                print(
                    f"Loaded up to {min(len(loaded_profiles_data), NUM_SUBPLOTS)} skin color profiles from {args.load_skin_colors}"
                )
            else:
                print(
                    f"Warning: Skin color file '{args.load_skin_colors}' does not contain a list of profiles. Using defaults."
                )
        except FileNotFoundError:
            print(
                f"Warning: Skin color file not found: {args.load_skin_colors}. Using defaults."
            )
        except json.JSONDecodeError:
            print(
                f"Warning: Error decoding JSON from {args.load_skin_colors}. Using defaults."
            )
        except Exception as e:
            print(
                f"Warning: Could not load skin colors from {args.load_skin_colors}: {e}. Using defaults."
            )

    # Create a figure and a 2x4 grid of subplots
    fig, axes = plt.subplots(2, 4, figsize=(26, 13), dpi=100)

    # Flatten the axes array for easy iteration
    axes = axes.flatten()

    if len(axes) != NUM_SUBPLOTS:
        print(
            f"Warning: Mismatch between NUM_SUBPLOTS ({NUM_SUBPLOTS}) and actual subplots ({len(axes)}). Adjusting."
        )
        NUM_SUBPLOTS = len(axes)
        # Ensure skin_color_profiles list matches the actual number of subplots
        if len(skin_color_profiles) < NUM_SUBPLOTS:
            skin_color_profiles.extend(
                [
                    SkinColorProfile()
                    for _ in range(NUM_SUBPLOTS - len(skin_color_profiles))
                ]
            )
        elif len(skin_color_profiles) > NUM_SUBPLOTS:
            skin_color_profiles = skin_color_profiles[:NUM_SUBPLOTS]

    # Configure each subplot to display the synthetic data
    for i, ax in enumerate(axes):
        # Initialize a new SyntheticDataGenerator for each subplot
        # For "gfx-shadow-ball" and "skin-fluff", the canvas will be created inside render_factory
        # For other modes, use the default skin template.
        if args.mode in [
            "gfx-shadow-ball",
            "skin-fluff",
            "cut-paste-demo",
            "flip-demo",
        ]:
            data_generator = SyntheticDataGenerator(
                np.zeros((1, 1), dtype=int), DISPLAY_COLOR_PALETTE
            )  # Dummy initial matrix, will be replaced

        elif args.mode in [
            "skin-default-06-0",
            "skin-default-06-1",
            "skin-default-16-0",
            "skin-default-16-1",
            "skin-default-04-0",
            "skin-default-04-1",
            "skin-default-14-0",
            "skin-default-14-1",
        ]:
            data_generator = SyntheticDataGenerator(
                DEFAULT_PLAYER_SKIN_FRAME_RIGHT_IDLE.copy(), DISPLAY_COLOR_PALETTE
            )
        else:
            data_generator = SyntheticDataGenerator(
                DEFAULT_PLAYER_SKIN_FRAME_DOWN_IDLE.copy(), DISPLAY_COLOR_PALETTE
            )

        # Initialize the SyntheticDataToolAPI for the current data generator
        tool_api = SyntheticDataToolAPI(data_generator)

        current_skin_profile = skin_color_profiles[i]

        # Apply rendering based on the selected mode
        render_factory(data_generator, tool_api, args.mode, current_skin_profile)

        # Render the data matrix to the current subplot using the tool API
        tool_api.render_data_matrix_to_subplot(ax, i)

        # Save individual graph data if requested
        if args.save_graph_data is not None:
            # data_matrix already contains compact IDs
            # Output the data_matrix as a list of lists (2D array)
            graph_frame_data = data_generator.data_matrix.tolist()
            # Build the color map based on data_generator.value_map (compact_id -> rgba)
            # The order in graph_color_map_list must match the compact_ids 0, 1, 2...
            num_colors = data_generator._next_value_id  # Total number of unique colors
            graph_color_map_list = [
                [0, 0, 0, 0]
            ] * num_colors  # Pre-allocate with a default

            # Ensure list is correctly sized and ordered
            # If value_map is not dense from 0 to num_colors-1, this needs care
            # However, _add_color_if_not_exists ensures compact_ids are 0, 1, ... _next_value_id-1
            for compact_id_key, rgba_tuple in sorted(data_generator.value_map.items()):
                if 0 <= compact_id_key < num_colors:
                    graph_color_map_list[compact_id_key] = list(rgba_tuple)

            output_data = {"FRAME": graph_frame_data, "COLOR": graph_color_map_list}

            filename = f"{args.save_graph_data}_{i}.json"

            try:
                with open(filename, "w") as f:
                    json.dump(output_data, f, indent=4)
                print(f"Saved graph data for subplot {i} to {filename}")
            except Exception as e:
                print(f"Error saving graph data for subplot {i} to {filename}: {e}")

    # Adjust layout to prevent titles/labels from overlapping
    plt.tight_layout()

    if args.save_skin_colors:
        profiles_data_to_save = [profile.to_dict() for profile in skin_color_profiles]
        try:
            with open(args.save_skin_colors, "w") as f:
                json.dump(profiles_data_to_save, f, indent=4)
            print(
                f"Saved {len(profiles_data_to_save)} skin color profiles to {args.save_skin_colors}"
            )
        except Exception as e:
            print(f"Error saving skin color profiles to {args.save_skin_colors}: {e}")

    # Display the plot only if --show is specified
    if args.show:
        plt.show()
    else:
        print("Graphics generated. Use --show to display.")
