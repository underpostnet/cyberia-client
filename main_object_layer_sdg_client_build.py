import subprocess
import os
import shutil


def execute_client_script(mode: str, output_dir: str = "build_output"):
    """
    Executes the main_object_layer_sdg_client.py script with the specified mode.

    Args:
        mode (str): The mode to run the client script in (e.g., "skin-default-0").
        output_dir (str): The directory to save graph data.
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


if __name__ == "__main__":
    output_directory = "build_output_sdg"
    if os.path.exists(output_directory):
        shutil.rmtree(output_directory)  # Clean up previous build
    os.makedirs(output_directory, exist_ok=True)

    for mode in [0, 1]:
        for direction in [2, 4, 6, 8]:
            for frame in [0, 1]:
                mode_name = f"skin-default-{mode}{direction}-{frame}"
                for graph_index in range(0, 8):

                    print(mode_name, graph_index)

                    # execute_client_script(mode_name, output_directory)
                    #         const RENDER_DATA = {
                    #       FRAMES: {
                    #         UP_IDLE: [],
                    #         DOWN_IDLE: [],
                    #         RIGHT_IDLE: [],
                    #         LEFT_IDLE: [],
                    #         UP_RIGHT_IDLE: [],
                    #         DOWN_RIGHT_IDLE: [],
                    #         UP_LEFT_IDLE: [],
                    #         DOWN_LEFT_IDLE: [],
                    #         DEFAULT_IDLE: [],
                    #         UP_WALKING: [],
                    #         DOWN_WALKING: [],
                    #         RIGHT_WALKING: [],
                    #         LEFT_WALKING: [],
                    #         UP_RIGHT_WALKING: [],
                    #         DOWN_RIGHT_WALKING: [],
                    #         UP_LEFT_WALKING: [],
                    #         DOWN_LEFT_WALKING: [],
                    #         NONE_IDLE: [],
                    #       },
                    #       COLORS: [],
                    #       FRAME_DURATION: 0.3,
                    #       IS_STATELESS: false,
                    #     };
                    # switch (direction) {
                    #           case '08':
                    #             RENDER_DATA.FRAMES.DOWN_IDLE.push(FRAMES);
                    #             RENDER_DATA.FRAMES.NONE_IDLE.push(FRAMES);
                    #             RENDER_DATA.FRAMES.DEFAULT_IDLE.push(FRAMES);
                    #             break;
                    #           case '18':
                    #             RENDER_DATA.FRAMES.DOWN_WALKING.push(FRAMES);
                    #             break;
                    #           case '02':
                    #             RENDER_DATA.FRAMES.UP_IDLE.push(FRAMES);
                    #             break;
                    #           case '12':
                    #             RENDER_DATA.FRAMES.UP_WALKING.push(FRAMES);
                    #             break;
                    #           case '04':
                    #             RENDER_DATA.FRAMES.LEFT_IDLE.push(FRAMES);
                    #             RENDER_DATA.FRAMES.UP_LEFT_IDLE.push(FRAMES);
                    #             RENDER_DATA.FRAMES.DOWN_LEFT_IDLE.push(FRAMES);
                    #             break;
                    #           case '14':
                    #             RENDER_DATA.FRAMES.LEFT_WALKING.push(FRAMES);
                    #             RENDER_DATA.FRAMES.UP_LEFT_WALKING.push(FRAMES);
                    #             RENDER_DATA.FRAMES.DOWN_LEFT_WALKING.push(FRAMES);
                    #             break;
                    #           case '06':
                    #             switch (objectLayerId) {
                    #               case 'people':
                    #                 RENDER_DATA.FRAMES.RIGHT_IDLE.push(FRAMES.reverse());
                    #                 RENDER_DATA.FRAMES.UP_RIGHT_IDLE.push(FRAMES.reverse());
                    #                 RENDER_DATA.FRAMES.DOWN_RIGHT_IDLE.push(FRAMES.reverse());
                    #                 break;

                    #               default:
                    #                 RENDER_DATA.FRAMES.RIGHT_IDLE.push(FRAMES);
                    #                 RENDER_DATA.FRAMES.UP_RIGHT_IDLE.push(FRAMES);
                    #                 RENDER_DATA.FRAMES.DOWN_RIGHT_IDLE.push(FRAMES);
                    #                 break;
                    #             }
                    #             break;
                    #           case '16':
                    #             switch (objectLayerId) {
                    #               case 'people':
                    #                 RENDER_DATA.FRAMES.RIGHT_WALKING.push(FRAMES.reverse());
                    #                 RENDER_DATA.FRAMES.UP_RIGHT_WALKING.push(FRAMES.reverse());
                    #                 RENDER_DATA.FRAMES.DOWN_RIGHT_WALKING.push(FRAMES.reverse());
                    #                 break;

                    #               default:
                    #                 RENDER_DATA.FRAMES.RIGHT_WALKING.push(FRAMES);
                    #                 RENDER_DATA.FRAMES.UP_RIGHT_WALKING.push(FRAMES);
                    #                 RENDER_DATA.FRAMES.DOWN_RIGHT_WALKING.push(FRAMES);
                    #                 break;
                    #             }

                    #             break;
                    #         }

                    # print(
                    #     f"\nAll specified modes processed. Check the '{output_directory}' directory for saved graph data."
                    # )
