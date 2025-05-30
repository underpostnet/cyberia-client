import json
import logging
import threading
import time
import websocket
import uuid

from raylibpy import Vector2, Color

from config import (
    NETWORK_OBJECT_SIZE,
    WORLD_WIDTH,
    WORLD_HEIGHT,
    SCREEN_WIDTH,
    SCREEN_HEIGHT,
    VIEWPORT_PADDING,
)
from network_state.network_object import NetworkObject
from network_state.network_state import NetworkState
from network_state.network_object_factory import NetworkObjectFactory
from network_state.astar import astar

logging.basicConfig(
    level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s"
)


class NetworkStateProxy:
    def __init__(
        self,
        host: str,
        port: int,
        ws_path: str,
        client_message_queue: list,
        client_connection_ready_event: threading.Event,
        client_player_id_setter,
        world_width: int,  # Added world_width
        world_height: int,  # Added world_height
        network_object_size: int,  # Added network_object_size
    ):
        self.websocket_url = f"ws://{host}:{port}{ws_path}"
        self.ws: websocket.WebSocketApp | None = None
        self.ws_thread: threading.Thread | None = None
        self.is_online: bool = False

        self.client_message_queue = client_message_queue
        self.client_connection_ready_event = client_connection_ready_event
        self.client_player_id_setter = client_player_id_setter

        # Store world dimensions and object size as attributes
        self.world_width = world_width
        self.world_height = world_height
        self.network_object_size = network_object_size

        self.network_object_factory = NetworkObjectFactory()
        self.offline_network_state = NetworkState(
            self.world_width,
            self.world_height,
            self.network_object_size,  # Use self attributes
        )
        self.my_player_id: str | None = None
        self.offline_player_path: list[dict[str, float]] = []

        self.offline_mode_update_thread = threading.Thread(
            target=self._offline_mode_update_loop, daemon=True
        )
        self.offline_mode_update_thread.start()

        self.client_viewport_center: Vector2 = Vector2(
            self.world_width / 2, self.world_height / 2
        )
        self.client_viewport_width: int = SCREEN_WIDTH
        self.client_viewport_height: int = SCREEN_HEIGHT
        self.viewport_padding: int = VIEWPORT_PADDING

    def _on_real_server_message(self, ws: websocket.WebSocketApp, message: str):
        try:
            data = json.loads(message)
            self._send_to_client(data)
            if data.get("type") == "player_assigned":
                self.my_player_id = data["player_id"]
                self.client_player_id_setter(self.my_player_id)
                logging.info(
                    f"Proxy received player ID: {self.my_player_id} from real server."
                )
            elif data.get("type") == "client_viewport_update":
                viewport_data = data.get("data", {})
                self.client_viewport_center.x = viewport_data.get(
                    "center_x", self.client_viewport_center.x
                )
                self.client_viewport_center.y = viewport_data.get(
                    "center_y", self.client_viewport_center.y
                )
        except json.JSONDecodeError:
            logging.error(f"Proxy failed to decode JSON message from server: {message}")
        except Exception as e:
            logging.exception(f"Proxy error in on_real_server_message: {e}")

    def _on_real_server_error(self, ws: websocket.WebSocketApp, error: Exception):
        logging.error(
            f"Real server WebSocket error: {error}. Switching to offline mode."
        )
        self.is_online = False
        self._send_offline_initial_state()
        self.client_connection_ready_event.set()

    def _on_real_server_close(
        self, ws: websocket.WebSocketApp, close_status_code: int, close_msg: str
    ):
        logging.info(
            f"Disconnected from real server: {close_status_code} - {close_msg}. Switching to offline mode."
        )
        self.is_online = False
        self._send_offline_initial_state()
        self.client_connection_ready_event.set()

    def _on_real_server_open(self, ws: websocket.WebSocketApp):
        logging.info("Connected to real server via Pure WebSocket!")
        self.is_online = True
        self.client_connection_ready_event.set()

    def _send_to_client(self, data: dict):
        with threading.Lock():
            self.client_message_queue.append(data)

    def connect(self):
        logging.info(f"Proxy connecting to real server at {self.websocket_url}")
        self.ws = websocket.WebSocketApp(
            self.websocket_url,
            on_open=self._on_real_server_open,
            on_message=self._on_real_server_message,
            on_error=self._on_real_server_error,
            on_close=self._on_real_server_close,
        )
        self.ws_thread = threading.Thread(target=self.ws.run_forever, daemon=True)
        self.ws_thread.start()

    def send_client_message(self, msg_type: str, payload: dict | None = None):
        if payload is None:
            payload = {}

        if msg_type == "client_viewport_update":
            self.client_viewport_center.x = payload.get(
                "center_x", self.client_viewport_center.x
            )
            self.client_viewport_center.y = payload.get(
                "center_y", self.client_viewport_center.y
            )

        if self.is_online and self.ws:
            message = {"type": msg_type, "data": payload}
            try:
                self.ws.send(json.dumps(message))
                logging.debug(f"Proxy sent message to real server: {msg_type}")
            except websocket._exceptions.WebSocketConnectionClosedException:
                logging.error(
                    "Real server WebSocket connection closed, cannot send message. Switching to offline."
                )
                self.is_online = False
                self._send_offline_initial_state()
            except Exception as e:
                logging.exception(
                    f"Proxy error sending message to real server: {e}. Switching to offline."
                )
                self.is_online = False
                self._send_offline_initial_state()
        else:
            logging.info(f"Proxy in offline mode. Handling client message: {msg_type}")
            self._handle_offline_client_message(msg_type, payload)

    def _handle_offline_client_message(self, msg_type: str, payload: dict):
        current_time = time.time()
        if msg_type == "client_move_request":
            target_x = payload.get("target_x")
            target_y = payload.get("target_y")
            if target_x is not None and target_y is not None:
                self._handle_offline_move_request(target_x, target_y, current_time)

    def _send_offline_initial_state(self):
        initial_state_data = self.network_object_factory.generate_initial_state_dict()

        player_id_found = False
        for obj_id, obj_data in initial_state_data["network_objects"].items():
            if obj_data.get("network_object_type") == "PLAYER":
                self.my_player_id = obj_id
                self.client_player_id_setter(self.my_player_id)
                logging.info(f"Proxy assigned offline player ID: {self.my_player_id}.")
                player_id_found = True
                break

        if not player_id_found:
            logging.warning(
                "No player object found in generated initial offline state."
            )

        self.offline_network_state.update_from_dict(
            initial_state_data["network_objects"]
        )

        filtered_objects = self._filter_objects_by_aoi(
            self.offline_network_state.get_all_network_objects()
        )
        initial_state_data["network_objects"] = {
            obj.obj_id: obj.to_dict() for obj in filtered_objects
        }

        self._send_to_client(initial_state_data)
        self._send_to_client(
            {"type": "player_assigned", "player_id": self.my_player_id}
        )
        logging.info("Proxy sent offline initial state to client.")

    def _filter_objects_by_aoi(
        self, objects: dict[str, NetworkObject]
    ) -> list[NetworkObject]:
        """Filters network objects to include only those within the client's Area of Interest,
        considering world wrapping."""
        aoi_half_width = (self.client_viewport_width / 2) + self.viewport_padding
        aoi_half_height = (self.client_viewport_height / 2) + self.viewport_padding

        # Calculate the "unwrapped" AoI boundaries around the player's center
        aoi_min_x_unwrapped = self.client_viewport_center.x - aoi_half_width
        aoi_max_x_unwrapped = self.client_viewport_center.x + aoi_half_width
        aoi_min_y_unwrapped = self.client_viewport_center.y - aoi_half_height
        aoi_max_y_unwrapped = self.client_viewport_center.y + aoi_half_height

        filtered = []
        for obj_id, obj in objects.items():
            # Check if the object (or any of its wrapped copies) is within the AoI
            # This is done by checking if the object's position, or its position +/- WORLD_WIDTH/HEIGHT
            # falls within the unwrapped AoI boundaries.
            is_in_aoi = False
            for x_offset in [-self.world_width, 0, self.world_width]:
                for y_offset in [-self.world_height, 0, self.world_height]:
                    test_x = obj.x + x_offset
                    test_y = obj.y + y_offset

                    if (aoi_min_x_unwrapped <= test_x < aoi_max_x_unwrapped) and (
                        aoi_min_y_unwrapped <= test_y < aoi_max_y_unwrapped
                    ):
                        is_in_aoi = True
                        break
                if is_in_aoi:
                    break

            if is_in_aoi or obj.obj_id == self.my_player_id:
                filtered.append(obj)
        return filtered

    def _offline_mode_update_loop(self):
        last_update_time = time.time()
        while True:
            time.sleep(0.01)
            current_time = time.time()
            delta_time = current_time - last_update_time
            last_update_time = current_time

            if not self.is_online:
                if self.my_player_id:
                    player_obj = self.offline_network_state.get_network_object(
                        self.my_player_id
                    )
                    if player_obj and player_obj.path:
                        player_obj.update_position(delta_time)
                        if player_obj.path_index >= len(player_obj.path):
                            player_obj.path = []
                            player_obj.path_index = 0
                            self.offline_player_path = []

                network_objects_data = {
                    obj_id: obj.to_dict()
                    for obj_id, obj in self.offline_network_state.get_all_network_objects().items()
                    if obj.is_persistent
                }

                filtered_objects = self._filter_objects_by_aoi(
                    self.offline_network_state.get_all_network_objects()
                )
                filtered_network_objects_data = {
                    obj.obj_id: obj.to_dict() for obj in filtered_objects
                }

                self._send_to_client(
                    {
                        "type": "network_state_update",
                        "network_objects": filtered_network_objects_data,
                    }
                )

    def _handle_offline_move_request(
        self, target_x: float, target_y: float, current_time: float
    ):
        if not self.my_player_id:
            logging.warning("Cannot handle offline move request: Player ID not set.")
            return

        player_obj = self.offline_network_state.get_network_object(self.my_player_id)
        if not player_obj:
            logging.warning(
                f"Player object with ID {self.my_player_id} not found in offline state."
            )
            return

        start_grid_x, start_grid_y = self.offline_network_state._world_to_grid_coords(
            player_obj.x, player_obj.y
        )
        end_grid_x, end_grid_y = self.offline_network_state._world_to_grid_coords(
            target_x, target_y
        )

        maze = self.offline_network_state.simplified_maze
        path_grid = astar(maze, (start_grid_y, start_grid_x), (end_grid_y, end_grid_x))

        if path_grid:
            path_world_coords = []
            for grid_y, grid_x in path_grid:
                world_x, world_y = self.offline_network_state._grid_to_world_coords(
                    grid_x, grid_y
                )
                path_world_coords.append({"X": world_x, "Y": world_y})

            player_obj.set_path(path_world_coords)
            self.offline_player_path = path_world_coords

            self._send_to_client(
                {
                    "type": "player_path_update",
                    "player_id": self.my_player_id,
                    "path": path_world_coords,
                }
            )
            logging.info(
                f"Offline path found for player {self.my_player_id}: {len(path_world_coords)} points."
            )
        else:
            logging.warning(
                f"No offline path found for player {self.my_player_id} from ({start_grid_x},{start_grid_y}) to ({end_grid_x},{end_grid_y})."
            )

    def close(self):
        if self.ws:
            self.ws.close()
            logging.info("Proxy WebSocket connection closed.")
