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
    MAZE_CELL_WORLD_SIZE,
)
from network_state.network_object import NetworkObject
from network_state.network_state import NetworkState
from network_state.network_object_factory import NetworkObjectFactory
from network_state.astar import astar  # For offline pathfinding

logging.basicConfig(
    level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s"
)


class NetworkStateProxy:
    """
    Acts as a proxy for the network state, handling communication with a real server
    or emulating server behavior if offline. It also manages the offline simulation
    of player movement.
    """

    def __init__(
        self,
        host: str,
        port: int,
        ws_path: str,
        client_message_queue: list,
        client_connection_ready_event: threading.Event,
        client_player_id_setter,
    ):
        self.websocket_url = f"ws://{host}:{port}{ws_path}"
        self.ws: websocket.WebSocketApp | None = None
        self.ws_thread: threading.Thread | None = None
        self.is_online: bool = False  # Tracks connection to the real server

        self.client_message_queue = client_message_queue
        self.client_connection_ready_event = client_connection_ready_event
        self.client_player_id_setter = client_player_id_setter

        # The proxy's factory is used only for generating initial persistent game state (players, walls)
        self.network_object_factory = NetworkObjectFactory()
        self.offline_network_state = NetworkState(
            WORLD_WIDTH, WORLD_HEIGHT, NETWORK_OBJECT_SIZE
        )
        self.my_player_id: str | None = None
        self.offline_player_target_pos: Vector2 | None = None
        self.offline_player_path: list[dict[str, float]] = []

        self.offline_mode_update_thread = threading.Thread(
            target=self._offline_mode_update_loop, daemon=True
        )
        self.offline_mode_update_thread.start()

    def _on_real_server_message(self, ws: websocket.WebSocketApp, message: str):
        """Callback for messages received from the real server."""
        try:
            data = json.loads(message)
            self._send_to_client(data)  # Forward all server messages to client
            if data.get("type") == "player_assigned":
                self.my_player_id = data["player_id"]
                self.client_player_id_setter(self.my_player_id)
                logging.info(
                    f"Proxy received player ID: {self.my_player_id} from real server."
                )
        except json.JSONDecodeError:
            logging.error(f"Proxy failed to decode JSON message from server: {message}")
        except Exception as e:
            logging.exception(f"Proxy error in on_real_server_message: {e}")

    def _on_real_server_error(self, ws: websocket.WebSocketApp, error: Exception):
        """Callback for errors from the real server connection."""
        logging.error(
            f"Real server WebSocket error: {error}. Switching to offline mode."
        )
        self.is_online = False
        self._send_offline_initial_state()  # Immediately send offline state to client
        self.client_connection_ready_event.set()  # Signal client is ready in offline mode

    def _on_real_server_close(
        self, ws: websocket.WebSocketApp, close_status_code: int, close_msg: str
    ):
        """Callback for real server connection closure."""
        logging.info(
            f"Disconnected from real server: {close_status_code} - {close_msg}. Switching to offline mode."
        )
        self.is_online = False
        self._send_offline_initial_state()  # Immediately send offline state to client
        self.client_connection_ready_event.set()  # Signal client is ready in offline mode

    def _on_real_server_open(self, ws: websocket.WebSocketApp):
        """Callback for successful real server connection."""
        logging.info("Connected to real server via Pure WebSocket!")
        self.is_online = True
        # Request initial state or player assignment if needed, or wait for server to send it
        # For now, we'll assume the server sends initial state and player_assigned message
        self.client_connection_ready_event.set()  # Signal client is ready

    def _send_to_client(self, data: dict):
        """Puts a message into the client's message queue."""
        with threading.Lock():  # Use a generic lock for the client's message queue
            self.client_message_queue.append(data)

    def connect(self):
        """Initiates the connection to the real server."""
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
        """
        Receives messages from the client and either forwards them to the real server
        or handles them in offline mode. GFX generation is now handled client-side.
        """
        if payload is None:
            payload = {}

        # Check if online and WebSocket is active before attempting to send
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
        """Handles client messages when in offline mode."""
        current_time = time.time()
        if msg_type == "client_move_request":
            target_x = payload.get("target_x")
            target_y = payload.get("target_y")
            if target_x is not None and target_y is not None:
                # This only handles the *actual* player movement path for offline mode
                self._handle_offline_move_request(target_x, target_y, current_time)
        # Other client messages can be handled here for offline mode if needed

    def _send_offline_initial_state(self):
        """Generates and sends an initial state for offline mode."""
        initial_state_data = self.network_object_factory.generate_initial_state_dict()

        # Always extract the player ID from the newly generated initial state
        # and update the proxy's my_player_id and inform the client.
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

        # Update the offline network state with the generated initial objects
        self.offline_network_state.update_from_dict(
            initial_state_data["network_objects"]
        )

        # Send the initial state to the client
        self._send_to_client(initial_state_data)
        self._send_to_client(
            {"type": "player_assigned", "player_id": self.my_player_id}
        )
        logging.info("Proxy sent offline initial state to client.")

    def _offline_mode_update_loop(self):
        """
        Simulates server updates in offline mode.
        This loop runs continuously to update the offline network state and send it to the client.
        """
        last_update_time = time.time()
        while True:
            time.sleep(0.01)  # Small delay to prevent busy-waiting
            current_time = time.time()
            delta_time = current_time - last_update_time
            last_update_time = current_time

            if not self.is_online:  # Only update offline state if not online
                # Update player position if a path is set
                if self.my_player_id:
                    player_obj = self.offline_network_state.get_network_object(
                        self.my_player_id
                    )
                    if player_obj and player_obj.path:
                        player_obj.update_position(delta_time)
                        if player_obj.path_index >= len(player_obj.path):
                            player_obj.path = []
                            player_obj.path_index = 0
                            self.offline_player_path = []  # Clear proxy's path as well

                # The proxy's offline_network_state should only contain persistent game objects (players, walls).
                # GFX objects (POINT_PATH, CLICK_POINTER) are handled client-side and should not be in this state.
                # Therefore, cleanup_expired_network_objects here would only apply to any non-persistent objects
                # the proxy *might* create, but in this architecture, it shouldn't create GFX objects.
                # We remove the explicit cleanup and message sending here, as client will handle its own GFX cleanup.

                # Send the current state of *persistent* offline objects to the client.
                # This ensures only actual game state is sent, not client-side GFX.
                network_objects_data = {
                    obj_id: obj.to_dict()
                    for obj_id, obj in self.offline_network_state.get_all_network_objects().items()
                    if obj.is_persistent  # Only send persistent objects
                }
                self._send_to_client(
                    {
                        "type": "network_state_update",
                        "network_objects": network_objects_data,
                    }
                )

    def _handle_offline_move_request(
        self, target_x: float, target_y: float, current_time: float
    ):
        """
        Calculates a path for the player in offline mode and updates the player's
        network object with this path. This path is for actual movement, not GFX.
        """
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
        path_grid = astar(
            maze, (start_grid_y, start_grid_x), (end_grid_y, end_grid_x)
        )  # astar expects (row, col)

        if path_grid:
            path_world_coords = []
            for grid_y, grid_x in path_grid:
                world_x, world_y = self.offline_network_state._grid_to_world_coords(
                    grid_x, grid_y
                )
                path_world_coords.append({"X": world_x, "Y": world_y})

            player_obj.set_path(path_world_coords)
            self.offline_player_path = path_world_coords  # Store for consistency

            # Send player path update to client (this is the actual movement path).
            # The client will use this path to render the player's movement
            # and to generate its own client-side POINT_PATH GFX.
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
        """Closes the WebSocket connection if open."""
        if self.ws:
            self.ws.close()
            logging.info("Proxy WebSocket connection closed.")
