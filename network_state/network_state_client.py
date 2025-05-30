import json
import logging
import threading
import time
import uuid

import websocket
from raylibpy import BLACK, MOUSE_BUTTON_LEFT, RAYWHITE, Vector2

from config import (
    CAMERA_SMOOTHNESS,
    NETWORK_OBJECT_SIZE,
    SCREEN_HEIGHT,
    SCREEN_WIDTH,
    WORLD_HEIGHT,
    WORLD_WIDTH,
    VIEWPORT_PADDING,
)
from object_layer.object_layer_data import ObjectLayerMode
from object_layer.object_layer_render import ObjectLayerRender
from network_state.network_object import NetworkObject
from network_state.network_state import NetworkState
from network_state.network_object_factory import (
    NetworkObjectFactory,
)
from network_state.network_state_proxy import NetworkStateProxy
from network_state.astar import astar  # Ensure this import is present and correct


logging.basicConfig(
    level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s"
)


class NetworkStateClient:
    def __init__(self, host: str, port: int, ws_path: str):
        self.host = host
        self.port = port
        self.ws_path = ws_path

        self.network_object_factory = NetworkObjectFactory()

        self.object_layer_render = ObjectLayerRender(
            screen_width=SCREEN_WIDTH,
            screen_height=SCREEN_HEIGHT,
            world_width=WORLD_WIDTH,
            world_height=WORLD_HEIGHT,
            network_object_size=NETWORK_OBJECT_SIZE,
            object_layer_data=self.network_object_factory.get_object_layer_data(),
            title="Python NetworkState Client",
            target_fps=60,
        )

        self.network_state = NetworkState(
            WORLD_WIDTH, WORLD_HEIGHT, NETWORK_OBJECT_SIZE
        )
        self.my_player_id: str | None = None
        self.my_network_object: NetworkObject | None = None

        self.message_queue: list[dict] = []
        self.message_queue_lock = threading.Lock()
        self.connection_ready_event = threading.Event()

        self.proxy = NetworkStateProxy(
            host=self.host,
            port=self.port,
            ws_path=self.ws_path,
            client_message_queue=self.message_queue,
            client_connection_ready_event=self.connection_ready_event,
            client_player_id_setter=self._set_my_player_id,
            world_width=WORLD_WIDTH,
            world_height=WORLD_HEIGHT,
            network_object_size=NETWORK_OBJECT_SIZE,
        )

    def _set_my_player_id(self, player_id: str):
        self.my_player_id = player_id

    def _process_queued_messages(self):
        with self.message_queue_lock:
            messages_to_process = list(self.message_queue)
            self.message_queue.clear()

        for data in messages_to_process:
            self._handle_proxy_message(data)

    def _handle_proxy_message(self, data: dict):
        msg_type = data.get("type")
        if msg_type == "network_state_update":
            if "network_objects" in data:
                object_layer_ids_to_remove_from_rendering_system = (
                    self.network_state.update_from_dict(data["network_objects"])
                )

                for (
                    obj_id,
                    object_layer_id,
                ) in object_layer_ids_to_remove_from_rendering_system:
                    self.object_layer_render.remove_object_layer_animation(
                        obj_id, object_layer_id
                    )

                with self.network_state.lock:
                    if (
                        self.my_player_id
                        and self.my_player_id in self.network_state.network_objects
                    ):
                        self.my_network_object = self.network_state.network_objects[
                            self.my_player_id
                        ]
                    else:
                        self.my_network_object = None
            else:
                logging.error(
                    f"Received 'network_state_update' message without 'network_objects' key: {data}"
                )
        elif msg_type == "player_assigned":
            logging.info(f"Client received assigned player ID: {self.my_player_id}.")
        elif msg_type == "message":
            logging.info(f"Proxy message: {data.get('text', 'No message text')}")
        elif msg_type == "player_path_update":
            player_id = data.get("player_id")
            path = data.get("path")
            if player_id and path:
                with self.network_state.lock:
                    player_obj = self.network_state.get_network_object(player_id)
                    if player_obj:
                        player_obj.set_path(path)
                        logging.info(f"Player {player_id} received path update: {path}")
                    else:
                        logging.warning(
                            f"Received path update for unknown player ID: {player_id}"
                        )
            else:
                logging.warning(f"Invalid player_path_update message: {data}")
        elif msg_type == "object_removed_from_rendering":
            obj_id = data.get("obj_id")
            object_layer_id = data.get("object_layer_id")
            if obj_id and object_layer_id:
                self.object_layer_render.remove_object_layer_animation(
                    obj_id, object_layer_id
                )
                with self.network_state.lock:
                    obj_to_remove = self.network_state.get_network_object(obj_id)
                    if obj_to_remove and obj_to_remove.is_persistent:
                        self.network_state.remove_network_object(obj_id)
                logging.debug(
                    f"Removed object {obj_id} with layer {object_layer_id} from rendering as requested by proxy."
                )
            else:
                logging.warning(
                    f"Invalid object_removed_from_rendering message: {data}"
                )
        else:
            logging.warning(
                f"Unknown message type received from proxy: {msg_type} - {data}"
            )

    def send_message_to_proxy(self, msg_type: str, payload: dict | None = None):
        self.proxy.send_client_message(msg_type, payload)

    def _generate_client_path_gfx_async(
        self, path_coords: list[dict[str, float]], current_time: float
    ):
        obj_ids_to_remove = []
        with self.network_state.lock:
            for obj_id, obj in self.network_state.network_objects.items():
                if obj.network_object_type == "POINT_PATH" and not obj.is_persistent:
                    obj_ids_to_remove.append(obj_id)
            for obj_id in obj_ids_to_remove:
                self.network_state.remove_network_object(obj_id)
        for obj_id in obj_ids_to_remove:
            self.object_layer_render.remove_object_layer_animation(obj_id, "POINT_PATH")

        delay_per_point = 0.02

        for point in path_coords:
            obj = self.network_object_factory.generate_point_path(
                [point], current_time
            )[0]
            with self.network_state.lock:
                self.network_state.add_or_update_network_object(obj)
            time.sleep(delay_per_point)
        logging.debug(
            f"Generated {len(path_coords)} client-side path GFX points asynchronously."
        )

    def run(self):
        self.proxy.connect()

        if not self.connection_ready_event.wait(timeout=10):
            logging.error("Failed to establish connection via proxy within timeout.")
            self.object_layer_render.close_window()
            self.proxy.close()
            return

        last_frame_time = time.time()

        while not self.object_layer_render.window_should_close():
            current_time = time.time()
            delta_time = current_time - last_frame_time
            last_frame_time = current_time

            self._process_queued_messages()

            object_layer_ids_to_remove_from_rendering_system = []
            with self.network_state.lock:
                object_layer_ids_to_remove_from_rendering_system = (
                    self.network_state.cleanup_expired_network_objects(current_time)
                )
            for (
                obj_id,
                object_layer_id,
            ) in object_layer_ids_to_remove_from_rendering_system:
                self.object_layer_render.remove_object_layer_animation(
                    obj_id, object_layer_id
                )

            object_movement_deltas = {}
            with self.network_state.lock:
                for obj_id, obj in self.network_state.network_objects.items():
                    # Update position and get effective movement delta
                    effective_dx, effective_dy = obj.update_position(delta_time)
                    object_movement_deltas[obj_id] = (effective_dx, effective_dy)

                    if obj.path and obj.path_index >= len(obj.path):
                        obj.path = []
                        obj.path_index = 0

            if self.object_layer_render.is_mouse_button_pressed(MOUSE_BUTTON_LEFT):
                world_mouse_pos = self.object_layer_render.get_world_mouse_position()
                logging.info(
                    f"Mouse clicked at world position: ({world_mouse_pos.x}, {world_mouse_pos.y})"
                )

                # Calculate grid coordinates for the exact clicked spot
                clicked_grid_x, clicked_grid_y = (
                    self.network_state._world_to_grid_coords(
                        world_mouse_pos.x, world_mouse_pos.y
                    )
                )

                # Generate click pointer regardless of obstacle status
                click_pointer_obj = self.network_object_factory.generate_click_pointer(
                    world_mouse_pos.x, world_mouse_pos.y, current_time
                )
                with self.network_state.lock:
                    self.network_state.add_or_update_network_object(click_pointer_obj)

                # Check if the clicked location is within maze bounds and is an obstacle
                if (
                    0 <= clicked_grid_y < len(self.network_state.simplified_maze)
                    and 0 <= clicked_grid_x < len(self.network_state.simplified_maze[0])
                    and self.network_state.simplified_maze[clicked_grid_y][
                        clicked_grid_x
                    ]
                    == 1
                ):
                    logging.warning(
                        f"Clicked on an obstacle at grid ({clicked_grid_x}, {clicked_grid_y}). Pathfinding skipped."
                    )
                else:
                    # If not an obstacle, proceed with move request and pathfinding
                    move_request_payload = {
                        "target_x": world_mouse_pos.x,
                        "target_y": world_mouse_pos.y,
                    }
                    self.send_message_to_proxy(
                        "client_move_request", move_request_payload
                    )

                    if self.my_network_object:
                        player_current_world_x = self.my_network_object.x
                        player_current_world_y = self.my_network_object.y

                        maze = self.network_state.simplified_maze
                        shortest_path_grid = None
                        min_path_length = float("inf")

                        # Consider 9 potential target locations for wrapping effect
                        for dx_wrap in [-1, 0, 1]:
                            for dy_wrap in [-1, 0, 1]:
                                candidate_target_x = (
                                    world_mouse_pos.x + dx_wrap * WORLD_WIDTH
                                )
                                candidate_target_y = (
                                    world_mouse_pos.y + dy_wrap * WORLD_HEIGHT
                                )

                                start_grid_x, start_grid_y = (
                                    self.network_state._world_to_grid_coords(
                                        player_current_world_x, player_current_world_y
                                    )
                                )
                                end_grid_x, end_grid_y = (
                                    self.network_state._world_to_grid_coords(
                                        candidate_target_x, candidate_target_y
                                    )
                                )

                                # Pass maze dimensions to the heuristic
                                current_path_grid = astar(
                                    maze,
                                    (start_grid_y, start_grid_x),
                                    (end_grid_y, end_grid_x),
                                )

                                if (
                                    current_path_grid
                                    and len(current_path_grid) < min_path_length
                                ):
                                    min_path_length = len(current_path_grid)
                                    shortest_path_grid = current_path_grid

                        if shortest_path_grid:
                            path_world_coords = []
                            prev_world_x, prev_world_y = (
                                player_current_world_x,
                                player_current_world_y,
                            )
                            for grid_y, grid_x in shortest_path_grid:
                                world_x, world_y = (
                                    self.network_state._grid_to_world_coords(
                                        grid_x, grid_y, prev_world_x, prev_world_y
                                    )
                                )
                                path_world_coords.append({"X": world_x, "Y": world_y})
                                prev_world_x, prev_world_y = world_x, world_y

                            threading.Thread(
                                target=self._generate_client_path_gfx_async,
                                args=(path_world_coords, current_time),
                                daemon=True,
                            ).start()
                        else:
                            logging.warning(
                                "No path found for client-side POINT_PATH GFX generation."
                            )
                    else:
                        logging.warning(
                            "Player object not available for client-side path GFX generation."
                        )

            self.object_layer_render.begin_drawing()
            self.object_layer_render.clear_background(RAYWHITE)

            if self.my_network_object:
                # Get the raw, wrapped position of the player
                player_raw_x = self.my_network_object.x
                player_raw_y = self.my_network_object.y

                # Get the currently rendered (smoothed) position
                smoothed_player_pos = (
                    self.object_layer_render.get_smoothed_object_position(
                        self.my_player_id
                    )
                )

                if smoothed_player_pos:
                    # Calculate the difference, considering potential wrap-around for snapping
                    diff_x_for_snap = player_raw_x - smoothed_player_pos.x
                    diff_y_for_snap = player_raw_y - smoothed_player_pos.y

                    # Adjust diff_x_for_snap for wrapping
                    # If the distance is greater than half the world, it means we've wrapped.
                    # Adjust the difference to be the "shorter" wrapped distance.
                    if abs(diff_x_for_snap) > WORLD_WIDTH / 2:
                        diff_x_for_snap = (player_raw_x % WORLD_WIDTH) - (
                            smoothed_player_pos.x % WORLD_WIDTH
                        )
                        if (
                            abs(diff_x_for_snap) > WORLD_WIDTH / 2
                        ):  # Check again after initial modulo
                            if diff_x_for_snap > 0:
                                diff_x_for_snap -= WORLD_WIDTH
                            else:
                                diff_x_for_snap += WORLD_WIDTH

                    if abs(diff_y_for_snap) > WORLD_HEIGHT / 2:
                        diff_y_for_snap = (player_raw_y % WORLD_HEIGHT) - (
                            smoothed_player_pos.y % WORLD_HEIGHT
                        )
                        if (
                            abs(diff_y_for_snap) > WORLD_HEIGHT / 2
                        ):  # Check again after initial modulo
                            if diff_y_for_snap > 0:
                                diff_y_for_snap -= WORLD_HEIGHT
                            else:
                                diff_y_for_snap += WORLD_HEIGHT

                    # If the adjusted difference is still large (e.g., more than one object size),
                    # it indicates a direct wrap-around that needs immediate camera snap.
                    # This threshold prevents snapping for normal smooth movement.
                    if (
                        abs(diff_x_for_snap) > NETWORK_OBJECT_SIZE
                        or abs(diff_y_for_snap) > NETWORK_OBJECT_SIZE
                    ):
                        # Direct jump (no smoothness)
                        self.object_layer_render.camera.target.x = (
                            player_raw_x + NETWORK_OBJECT_SIZE / 2
                        )
                        self.object_layer_render.camera.target.y = (
                            player_raw_y + NETWORK_OBJECT_SIZE / 2
                        )
                    else:
                        # Otherwise, apply smooth camera movement
                        self.object_layer_render.update_camera_target(
                            Vector2(
                                player_raw_x + NETWORK_OBJECT_SIZE / 2,
                                player_raw_y + NETWORK_OBJECT_SIZE / 2,
                            ),
                            smoothness=CAMERA_SMOOTHNESS,
                        )

                    self.send_message_to_proxy(
                        "client_viewport_update",
                        {
                            "center_x": self.object_layer_render.camera.target.x,
                            "center_y": self.object_layer_render.camera.target.y,
                        },
                    )
                else:
                    # Fallback if smoothed_player_pos is not yet available, snap directly
                    self.object_layer_render.camera.target.x = (
                        self.my_network_object.x + NETWORK_OBJECT_SIZE / 2
                    )
                    self.object_layer_render.camera.target.y = (
                        self.my_network_object.y + NETWORK_OBJECT_SIZE / 2
                    )
                    self.send_message_to_proxy(
                        "client_viewport_update",
                        {
                            "center_x": self.object_layer_render.camera.target.x,
                            "center_y": self.object_layer_render.camera.target.y,
                        },
                    )

            self.object_layer_render.begin_camera_mode()

            with self.network_state.lock:
                for obj_id, obj in self.network_state.network_objects.items():
                    effective_dx, effective_dy = object_movement_deltas.get(
                        obj_id, (0.0, 0.0)
                    )
                    self.object_layer_render.draw_network_object(
                        obj, current_time, effective_dx, effective_dy
                    )

            self.object_layer_render.end_camera_mode()

            with self.network_state.lock:
                if self.my_network_object:
                    self.object_layer_render.draw_text(
                        f"My Player ID: {self.my_player_id}", 10, 10, 20, BLACK
                    )
                    self.object_layer_render.draw_text(
                        f"My Pos: ({int(self.my_network_object.x)}, {int(self.my_network_object.y)})",
                        10,
                        40,
                        20,
                        BLACK,
                    )
                else:
                    self.object_layer_render.draw_text(
                        "Connecting...", 10, 10, 20, BLACK
                    )

            frame_time = self.object_layer_render.get_frame_time()
            if frame_time > 0:
                self.object_layer_render.draw_text(
                    f"FPS: {int(1.0 / frame_time)}", 10, SCREEN_HEIGHT - 30, 20, BLACK
                )
            else:
                self.object_layer_render.draw_text(
                    "FPS: N/A", 10, SCREEN_HEIGHT - 30, 20, BLACK
                )

            self.object_layer_render.update_all_active_object_layer_animations(
                delta_time, current_time
            )

            self.object_layer_render.end_drawing()

            time.sleep(0.001)

        self.proxy.close()
        self.object_layer_render.close_window()
        logging.info("Client window closed.")
