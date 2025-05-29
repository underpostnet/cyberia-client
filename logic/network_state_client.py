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
)
from object_layer.object_layer_data import ObjectLayerMode, Direction
from object_layer.object_layer_render import ObjectLayerRender
from logic.network_object import NetworkObject
from logic.network_state import NetworkState
from logic.network_object_factory import NetworkObjectFactory

logging.basicConfig(
    level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s"
)


class NetworkStateClient:
    def __init__(self, host: str, port: int, ws_path: str):
        self.host = host
        self.port = port
        self.ws_path = ws_path
        self.websocket_url = f"ws://{self.host}:{self.port}{self.ws_path}"
        self.ws: websocket.WebSocketApp | None = None
        self.ws_thread: threading.Thread | None = None

        self.network_object_factory = NetworkObjectFactory()
        object_layer_data = self.network_object_factory.get_object_layer_data()

        self.object_layer_render = ObjectLayerRender(
            screen_width=SCREEN_WIDTH,
            screen_height=SCREEN_HEIGHT,
            world_width=WORLD_WIDTH,
            world_height=WORLD_HEIGHT,
            network_object_size=NETWORK_OBJECT_SIZE,
            object_layer_data=object_layer_data,
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

        self.my_player_path_for_gfx: list[dict[str, float]] = []

    def _on_message(self, ws: websocket.WebSocketApp, message: str):
        try:
            data = json.loads(message)
            with self.message_queue_lock:
                self.message_queue.append(data)
        except json.JSONDecodeError:
            logging.error(f"Failed to decode JSON message: {message}")
        except Exception as e:
            logging.exception(f"Error in on_message: {e}")

    def _on_error(self, ws: websocket.WebSocketApp, error: Exception):
        logging.error(f"WebSocket error: {error}")
        self.connection_ready_event.clear()

    def _on_close(
        self, ws: websocket.WebSocketApp, close_status_code: int, close_msg: str
    ):
        logging.info(f"Disconnected from server: {close_status_code} - {close_msg}")
        self.my_player_id = None
        self.my_network_object = None
        self.ws = None
        self.connection_ready_event.clear()

    def _on_open(self, ws: websocket.WebSocketApp):
        logging.info("Connected to server via Pure WebSocket!")
        self.connection_ready_event.set()

        initial_state_data = self.network_object_factory.generate_initial_state_dict()
        with self.message_queue_lock:
            self.message_queue.append(initial_state_data)

    def _process_queued_messages(self):
        with self.message_queue_lock:
            messages_to_process = list(self.message_queue)
            self.message_queue.clear()

        for data in messages_to_process:
            self._handle_server_message(data)

    def _handle_server_message(self, data: dict):
        msg_type = data.get("type")
        logging.debug(f"Handling message of type: {msg_type}, data: {data}")
        if msg_type == "network_state_update":
            # Changed to expect "network_objects" key from the server
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
                logging.debug("Network state updated.")
            else:
                logging.error(
                    f"Received 'network_state_update' message without 'network_objects' key: {data}"
                )
        elif msg_type == "player_assigned":
            self.my_player_id = data["player_id"]
            logging.info(f"Assigned player ID: {self.my_player_id} from server.")
        elif msg_type == "message":
            logging.info(f"Server message: {data.get('text', 'No message text')}")
        elif msg_type == "player_path_update":
            player_id = data.get("player_id")
            path = data.get("path")
            if player_id and path:
                with self.network_state.lock:
                    player_obj = self.network_state.get_network_object(player_id)
                    if player_obj:
                        player_obj.set_path(path)
                        if player_id == self.my_player_id:
                            self.my_player_path_for_gfx = path
                            logging.debug(
                                f"Stored path for GFX: {len(self.my_player_path_for_gfx)} points"
                            )
                            self._generate_path_gfx(
                                self.my_player_path_for_gfx, time.time()
                            )
                        logging.info(f"Player {player_id} received path update: {path}")
                    else:
                        logging.warning(
                            f"Received path update for unknown player ID: {player_id}"
                        )
            else:
                logging.warning(f"Invalid player_path_update message: {data}")
        else:
            logging.warning(f"Unknown message type received: {msg_type} - {data}")

    def send_message(self, msg_type: str, payload: dict | None = None):
        if self.ws and self.connection_ready_event.is_set() and self.my_player_id:
            message = {"type": msg_type, "data": payload if payload is not None else {}}
            try:
                self.ws.send(json.dumps(message))
            except websocket._exceptions.WebSocketConnectionClosedException:
                logging.error("WebSocket connection is closed, cannot send message.")
            except Exception as e:
                logging.exception(f"Error sending message: {e}")
        else:
            logging.warning(
                "Cannot send move request: Player not assigned yet or WebSocket not connected."
            )

    def _generate_path_gfx(self, path: list[dict[str, float]], current_time: float):
        object_layer_ids_to_remove_from_rendering_system = []

        obj_ids_to_remove = [
            obj_id
            for obj_id, obj in self.network_state.network_objects.items()
            if obj.network_object_type == "POINT_PATH"
            and obj_id.startswith(f"path_point_{self.my_player_id}")
        ]
        for obj_id in obj_ids_to_remove:
            obj_to_remove = self.network_state.remove_network_object(obj_id)
            if obj_to_remove and obj_to_remove.object_layer_ids:
                for object_layer_id in obj_to_remove.object_layer_ids:
                    object_layer_ids_to_remove_from_rendering_system.append(
                        (obj_id, object_layer_id)
                    )

        logging.debug(
            f"Clearing {len(object_layer_ids_to_remove_from_rendering_system)} old path points."
        )

        new_path_network_objects = self.network_object_factory.generate_point_path(
            path, current_time
        )
        for obj in new_path_network_objects:
            self.network_state.add_or_update_network_object(obj)

        for obj_id, object_layer_id in object_layer_ids_to_remove_from_rendering_system:
            self.object_layer_render.remove_object_layer_animation(
                obj_id, object_layer_id
            )

    def run(self):
        logging.info(f"Connecting to WebSocket server at {self.websocket_url}")

        self.ws = websocket.WebSocketApp(
            self.websocket_url,
            on_open=self._on_open,
            on_message=self._on_message,
            on_error=self._on_error,
            on_close=self._on_close,
        )

        self.ws_thread = threading.Thread(target=self.ws.run_forever, daemon=True)
        self.ws_thread.start()

        if not self.connection_ready_event.wait(timeout=5):
            logging.error("Failed to establish WebSocket connection within timeout.")
            self.object_layer_render.close_window()
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
                    dx, dy = obj.update_position(delta_time)
                    object_movement_deltas[obj_id] = (dx, dy)

                    if obj.path and obj.path_index >= len(obj.path):
                        obj.path = []
                        obj.path_index = 0

            if self.object_layer_render.is_mouse_button_pressed(MOUSE_BUTTON_LEFT):
                world_mouse_pos = self.object_layer_render.get_world_mouse_position()
                logging.info(
                    f"Mouse clicked at world position: ({world_mouse_pos.x}, {world_mouse_pos.y})"
                )
                move_request_payload = {
                    "target_x": world_mouse_pos.x,
                    "target_y": world_mouse_pos.y,
                }
                self.send_message("client_move_request", move_request_payload)

                object_layer_ids_to_remove_from_rendering_system_click = []
                with self.network_state.lock:
                    obj_ids_to_remove = [
                        obj_id
                        for obj_id, obj in self.network_state.network_objects.items()
                        if obj.network_object_type == "CLICK_POINTER"
                    ]
                    for obj_id in obj_ids_to_remove:
                        obj_to_remove = self.network_state.remove_network_object(obj_id)
                        if obj_to_remove and obj_to_remove.object_layer_ids:
                            for object_layer_id in obj_to_remove.object_layer_ids:
                                object_layer_ids_to_remove_from_rendering_system_click.append(
                                    (obj_id, object_layer_id)
                                )

                    new_click_pointer = (
                        self.network_object_factory.generate_click_pointer(
                            world_mouse_pos.x, world_mouse_pos.y, current_time
                        )
                    )
                    self.network_state.add_or_update_network_object(new_click_pointer)
                    logging.debug(
                        "Created new click pointer via network object factory."
                    )

                for (
                    obj_id,
                    object_layer_id,
                ) in object_layer_ids_to_remove_from_rendering_system_click:
                    self.object_layer_render.remove_object_layer_animation(
                        obj_id, object_layer_id
                    )

            self.object_layer_render.begin_drawing()
            self.object_layer_render.clear_background(RAYWHITE)
            self.object_layer_render.begin_camera_mode()

            self.object_layer_render.draw_grid()

            with self.network_state.lock:
                for obj_id, obj in self.network_state.network_objects.items():
                    dx, dy = object_movement_deltas.get(obj_id, (0.0, 0.0))
                    self.object_layer_render.draw_network_object(
                        obj, current_time, dx, dy
                    )

            self.object_layer_render.end_camera_mode()

            with self.network_state.lock:
                if self.my_network_object:
                    self.object_layer_render.update_camera_target(
                        Vector2(
                            self.my_network_object.x + NETWORK_OBJECT_SIZE / 2,
                            self.my_network_object.y + NETWORK_OBJECT_SIZE / 2,
                        ),
                        smoothness=CAMERA_SMOOTHNESS,
                    )
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

        if self.ws:
            self.ws.close()
        logging.info("WebSocket connection closed.")
        self.object_layer_render.close_window()
        logging.info("Client window closed.")
