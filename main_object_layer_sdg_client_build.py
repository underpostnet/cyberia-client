import subprocess
import os
import shutil
import json
import copy
import argparse
from typing import Union  # For Python < 3.10 compatibility


def execute_client_script(
    mode: str,
    output_dir: str = "build_output",
    skin_colors_file_for_subprocess: Union[str, None] = None,
):
    """
    Executes the main_object_layer_sdg_client.py script with the specified mode.

    Args:
        mode (str): The mode to run the client script in (e.g., "skin-default-0").
        output_dir (str): The directory to save graph data.
        skin_colors_file_for_subprocess (str | None): Path to the JSON file containing
                                                      skin color profiles to be passed to
                                                      the client script.
    """
    script_path = "main_object_layer_sdg_client.py"
    save_graph_data_prefix = os.path.join(output_dir, mode)

    command = [
        "python",
        script_path,
        "--mode",
        mode,
        "--save-graph-data",
        save_graph_data_prefix,
        # "--show" # Typically not used in build scripts to avoid multiple windows
    ]

    if skin_colors_file_for_subprocess:
        command.append("--load-skin-colors")
        command.append(skin_colors_file_for_subprocess)

    print(f"Executing: {' '.join(command)}")
    try:
        result = subprocess.run(command, check=True, capture_output=True, text=True)
        print(f"Successfully executed for mode: {mode}")
        if result.stdout:
            print("Output:\n", result.stdout)
    except subprocess.CalledProcessError as e:
        print(f"Error executing script for mode: {mode}")
        print("Command:", e.cmd)
        print("Return code:", e.returncode)
        print("Output (stdout):\n", e.stdout)
        print("Output (stderr):\n", e.stderr)
    except FileNotFoundError:
        print(
            f"Error: The script '{script_path}' was not found. Make sure it's in the correct path."
        )


def generate_random_seed_data():
    """Generates random integer values for SEED_DATA."""
    # Ensure random is imported if not already at the top level
    import random

    return {
        "EFFECT": random.randint(0, 10),
        "RESISTANCE": random.randint(0, 10),
        "AGILITY": random.randint(0, 10),
        "RANGE": random.randint(0, 10),
        "INTELLIGENCE": random.randint(0, 10),
        "UTILITY": random.randint(0, 10),
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Build script to generate consolidated player skin animation data."
    )
    parser.add_argument(
        "--base-profile-source",
        type=str,
        required=True,
        help="Base name of the JSON file in the current directory containing an array of 8 skin color profiles (e.g., RAVE for RAVE.json).",
    )
    args = parser.parse_args()

    RENDER_DATA_SCHEME = {
        "FRAMES": {
            "UP_IDLE": [],
            "DOWN_IDLE": [],
            "RIGHT_IDLE": [],
            "LEFT_IDLE": [],
            "UP_RIGHT_IDLE": [],
            "DOWN_RIGHT_IDLE": [],
            "UP_LEFT_IDLE": [],
            "DOWN_LEFT_IDLE": [],
            "DEFAULT_IDLE": [],
            "UP_WALKING": [],
            "DOWN_WALKING": [],
            "RIGHT_WALKING": [],
            "LEFT_WALKING": [],
            "UP_RIGHT_WALKING": [],
            "DOWN_RIGHT_WALKING": [],
            "UP_LEFT_WALKING": [],
            "DOWN_LEFT_WALKING": [],
            "NONE_IDLE": [],
        },
        "COLORS": [],
        "FRAME_DURATION": 0.3,
        "IS_STATELESS": False,
    }

    base_profile_source_name = args.base_profile_source
    base_profile_filepath = f"{base_profile_source_name}.json"

    all_base_profiles = []
    try:
        with open(base_profile_filepath, "r") as f:
            all_base_profiles = json.load(f)
        if not (isinstance(all_base_profiles, list) and len(all_base_profiles) == 8):
            print(
                f"Error: Base profile file '{base_profile_filepath}' must contain a list of 8 profiles."
            )
            exit(1)
        print(f"Successfully loaded 8 base profiles from '{base_profile_filepath}'.")
    except FileNotFoundError:
        print(f"Error: Base profile file not found: '{base_profile_filepath}'.")
        exit(1)
    except json.JSONDecodeError:
        print(f"Error: Could not decode JSON from '{base_profile_filepath}'.")
        exit(1)
    except Exception as e:
        print(f"Error loading base profiles from '{base_profile_filepath}': {e}")
        exit(1)

    output_skin_dir = "object_layer/skin"
    os.makedirs(output_skin_dir, exist_ok=True)

    intermediate_frames_dir = "build_output_sdg_frames_temp"

    print(f"\nGenerating 8 skin files for base profile: {base_profile_source_name}")

    # Prepare intermediate directory for client's output frames
    if os.path.exists(intermediate_frames_dir):
        shutil.rmtree(intermediate_frames_dir)
    os.makedirs(intermediate_frames_dir, exist_ok=True)

    # RENDER_DATA will hold the 8 variations for the skin ID
    RENDER_DATA_for_skin_id = [copy.deepcopy(RENDER_DATA_SCHEME) for _ in range(8)]

    # Iterate through different animation configurations (modes)
    for mode_val in [0, 1]:  # 0 for IDLE, 1 for WALKING
        for direction_val in [8, 2, 4, 6]:  # 8:DOWN, 2:UP, 4:LEFT, 6:RIGHT
            for frame_val in [0, 1]:  # Two animation frames for each state
                mode_name = f"skin-default-{mode_val}{direction_val}-{frame_val}"
                print(
                    f"    Executing client for mode: {mode_name} (will generate 8 variation files)"
                )

                # Execute the client script ONCE for the current mode_name.
                # This will generate 8 files: <mode_name>_0.json to <mode_name>_7.json
                execute_client_script(
                    mode_name, intermediate_frames_dir, base_profile_filepath
                )

                # Now, process the 8 files generated by the single client execution
                for graph_index in range(8):
                    input_json_filename = f"{mode_name}_{graph_index}.json"
                    input_json_path = os.path.join(
                        intermediate_frames_dir, input_json_filename
                    )

                    if not os.path.exists(input_json_path):
                        print(
                            f"      ERROR: Expected file {input_json_path} not found. Skipping this frame for variation {graph_index}."
                        )
                        continue

                    try:
                        with open(input_json_path, "r") as f:
                            loaded_data = json.load(f)

                        loaded_frame_data = loaded_data.get("FRAME")
                        loaded_color_data = loaded_data.get("COLOR")

                        if loaded_frame_data is None or loaded_color_data is None:
                            print(
                                f"      ERROR: FRAME or COLOR data missing in {input_json_path}. Skipping for variation {graph_index}."
                            )
                            os.remove(input_json_path)
                            continue

                        if not RENDER_DATA_for_skin_id[graph_index]["COLORS"]:
                            RENDER_DATA_for_skin_id[graph_index][
                                "COLORS"
                            ] = loaded_color_data

                        base_direction_str = ""
                        if direction_val == 8:
                            base_direction_str = "DOWN"
                        elif direction_val == 2:
                            base_direction_str = "UP"
                        elif direction_val == 4:
                            base_direction_str = "LEFT"
                        elif direction_val == 6:
                            base_direction_str = "RIGHT"

                        anim_type_str = "IDLE" if mode_val == 0 else "WALKING"
                        primary_anim_key = f"{base_direction_str}_{anim_type_str}"

                        RENDER_DATA_for_skin_id[graph_index]["FRAMES"][
                            primary_anim_key
                        ].append(loaded_frame_data)

                        if primary_anim_key == "DOWN_IDLE":
                            RENDER_DATA_for_skin_id[graph_index]["FRAMES"][
                                "DEFAULT_IDLE"
                            ].append(loaded_frame_data)
                            RENDER_DATA_for_skin_id[graph_index]["FRAMES"][
                                "NONE_IDLE"
                            ].append(loaded_frame_data)

                        if anim_type_str == "IDLE":
                            if base_direction_str == "LEFT":
                                RENDER_DATA_for_skin_id[graph_index]["FRAMES"][
                                    "UP_LEFT_IDLE"
                                ].append(loaded_frame_data)
                                RENDER_DATA_for_skin_id[graph_index]["FRAMES"][
                                    "DOWN_LEFT_IDLE"
                                ].append(loaded_frame_data)
                            elif base_direction_str == "RIGHT":
                                RENDER_DATA_for_skin_id[graph_index]["FRAMES"][
                                    "UP_RIGHT_IDLE"
                                ].append(loaded_frame_data)
                                RENDER_DATA_for_skin_id[graph_index]["FRAMES"][
                                    "DOWN_RIGHT_IDLE"
                                ].append(loaded_frame_data)
                        elif anim_type_str == "WALKING":
                            if base_direction_str == "LEFT":
                                RENDER_DATA_for_skin_id[graph_index]["FRAMES"][
                                    "UP_LEFT_WALKING"
                                ].append(loaded_frame_data)
                                RENDER_DATA_for_skin_id[graph_index]["FRAMES"][
                                    "DOWN_LEFT_WALKING"
                                ].append(loaded_frame_data)
                            elif base_direction_str == "RIGHT":
                                RENDER_DATA_for_skin_id[graph_index]["FRAMES"][
                                    "UP_RIGHT_WALKING"
                                ].append(loaded_frame_data)
                                RENDER_DATA_for_skin_id[graph_index]["FRAMES"][
                                    "DOWN_RIGHT_WALKING"
                                ].append(loaded_frame_data)

                        os.remove(input_json_path)

                    except FileNotFoundError:
                        print(
                            f"      ERROR: File {input_json_path} not found after execution. Skipping."
                        )
                    except json.JSONDecodeError:
                        print(
                            f"      ERROR: Could not decode JSON from {input_json_path}. Skipping."
                        )
                        if os.path.exists(input_json_path):
                            os.remove(input_json_path)
                    except Exception as e:
                        print(
                            f"      ERROR: An unexpected error occurred while processing {input_json_path}: {e}. Skipping."
                        )
                        if os.path.exists(input_json_path):
                            os.remove(input_json_path)

    # Generate 8 individual skin files
    for i in range(8):
        current_render_data_for_file = RENDER_DATA_for_skin_id[i]
        # SEED_DATA is generated uniquely for each of the 8 skin files
        current_seed_data_for_file = generate_random_seed_data()

        final_output_data_for_this_file = {
            "TYPE": "SKIN",
            "RENDER_DATA": current_render_data_for_file,  # Single RENDER_DATA object
            "SEED_DATA": current_seed_data_for_file,
        }

        # Filename format: object_layer_data_rave_0.json, object_layer_data_rave_1.json, etc.
        file_suffix = f"{base_profile_source_name.lower()}_{i}"
        output_filename = os.path.join(
            output_skin_dir, f"object_layer_data_{file_suffix}.json"
        )

        print(f"\n  Generating skin file: {output_filename}")
        try:
            with open(output_filename, "w") as f:
                json.dump(final_output_data_for_this_file, f, indent=4)
            print(f"    Successfully generated skin definition: {output_filename}")
        except Exception as e:
            print(f"    Error saving final data to {output_filename}: {e}")

    # Final cleanup of intermediate frames directory
    try:
        if os.path.exists(intermediate_frames_dir):
            shutil.rmtree(intermediate_frames_dir)
            print(f"\nCleaned up intermediate directory: {intermediate_frames_dir}")
    except Exception as e:
        print(
            f"\nWarning: Could not clean up intermediate directory {intermediate_frames_dir}: {e}"
        )

    print(
        f"\nBuild process finished. Check '{output_skin_dir}' for generated skin files."
    )
