import json
import logging
import threading
import time
import websocket
from typing import Union
import uuid
import random
import math  # Import math for trigonometric functions

from pyray import Vector2, Color

from config import (
    NETWORK_OBJECT_SIZE,
    WORLD_WIDTH,
    WORLD_HEIGHT,
)
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
        self.ws: Union[websocket.WebSocketApp, None] = None  # type: ignore
        self.ws_thread: Union[threading.Thread, None] = None  # type: ignore
        self.is_online: bool = False  # Tracks connection to the real server

        self.client_message_queue = client_message_queue
        self.client_connection_ready_event = client_connection_ready_event
        self.client_player_id_setter = client_player_id_setter

        # The proxy's factory is used only for generating initial persistent game state (players, walls) # type: ignore # This comment is for mypy, not part of the code
        self.network_object_factory = NetworkObjectFactory()
        self.offline_network_state = NetworkState(
            WORLD_WIDTH, WORLD_HEIGHT, NETWORK_OBJECT_SIZE
        )
        self.current_channel_id: str = "channel_alpha"  # Default channel
        self.my_player_id: Union[str, None] = None
        self.offline_player_target_pos: Union[Vector2, None] = None
        self.offline_player_path: list[dict[str, float]] = []  # type: ignore

        self.offline_mode_update_thread = threading.Thread(
            target=self._offline_mode_update_loop, daemon=True
        )
        self.offline_mode_update_thread.start()

    def _on_real_server_message(self, ws: websocket.WebSocketApp, message: str):
        """Callback for messages received from the real server."""
        try:
            data = json.loads(message)
            self._send_to_client(data)  # Forward all server messages to client
            msg_type = data.get("type")
            if msg_type == "player_assigned":
                self.my_player_id = data["player_id"]
                self.client_player_id_setter(self.my_player_id)
                logging.info(
                    f"Proxy received player ID: {self.my_player_id} from real server."
                )
            elif msg_type == "channel_assigned":  # If server assigns channel
                self.current_channel_id = data.get(
                    "channel_id", self.current_channel_id
                )
                logging.info(
                    f"Proxy assigned to channel: {self.current_channel_id} by real server."
                )
            # Potentially handle other specific real server messages if needed,
            # though generic forwarding covers server_chat_message.
            # Example: if msg_type == "server_chat_message":
            # logging.debug(f"Proxy received server_chat_message: {data}")
            # No special handling needed here as it's already sent to client.
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

    def send_client_message(self, msg_type: str, payload: Union[dict, None] = None):  # type: ignore
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
        elif msg_type == "client_chat_message":
            room_id = payload.get("room_id")
            text = payload.get("text")
            if room_id and text:
                sender_name = self.my_player_id if self.my_player_id else "OfflineUser"
                chat_message_response = {
                    "type": "server_chat_message",
                    "data": {
                        "room_id": room_id,
                        "sender": sender_name,
                        "text": text,
                        "time": time.strftime("%H:%M", time.localtime(current_time)),
                    },
                }
                self._send_to_client(chat_message_response)
                logging.info(
                    f"Proxy offline: processed client_chat_message for room {room_id}"
                )
        elif msg_type == "client_change_channel_request":
            new_channel_id = payload.get("channel_id")
            if new_channel_id and new_channel_id != self.current_channel_id:
                logging.info(
                    f"Proxy offline: Received request to change channel to {new_channel_id}"
                )
                self.current_channel_id = new_channel_id
                # Reset network state for the new channel
                self.offline_network_state = NetworkState(
                    WORLD_WIDTH, WORLD_HEIGHT, NETWORK_OBJECT_SIZE
                )
                self.my_player_id = None  # Player ID will be reassigned
                self.offline_player_path = []
                self.offline_player_target_pos = None
                self._send_offline_initial_state()  # Send new initial state for the new channel
                # Client needs to be aware that the world has changed.
                # The new network_state_update and player_assigned messages should handle this.
            elif not new_channel_id:
                logging.warning(
                    "Proxy offline: client_change_channel_request missing channel_id."
                )
            else:  # new_channel_id == self.current_channel_id
                logging.info(
                    f"Proxy offline: Already on channel {new_channel_id}. No change."
                )

    def _send_offline_initial_state(self):
        """Generates and sends an initial state for offline mode."""

        # Always extract the player ID from the newly generated initial state
        # and update the proxy's my_player_id and inform the client.
        # The factory now generates based on self.current_channel_id
        initial_state_data = self.network_object_factory.generate_initial_state_dict(
            self.current_channel_id
        )
        player_id_found = False  # Reset for this function call

        # Update the offline network state with the generated initial objects
        # This also creates the NetworkObject instances in offline_network_state
        self.offline_network_state.update_from_dict(
            initial_state_data["network_objects"]
        )

        for obj_id, obj in self.offline_network_state.get_all_network_objects().items():
            if obj.network_object_type == "PLAYER":
                self.my_player_id = obj_id
                self.client_player_id_setter(self.my_player_id)
                logging.info(f"Proxy assigned offline player ID: {self.my_player_id}.")
                player_id_found = True
            elif obj.network_object_type == "BOT-QUEST-PROVIDER":
                # Configure autonomous movement for each bot directly on the NetworkObject
                # Using default values from NetworkObject.configure_autonomous_movement
                obj.configure_autonomous_movement(
                    initial_pos=Vector2(obj.x, obj.y)  # Use current position as initial
                )
                logging.info(
                    f"Proxy configured BOT-QUEST-PROVIDER {obj_id} for autonomous movement."
                )

        if not player_id_found:
            logging.warning(
                "No player object found in generated initial offline state."
            )

        # Send the initial state to the client
        self._send_to_client(initial_state_data)
        self._send_to_client(
            {
                "type": "player_assigned",
                "player_id": self.my_player_id,
                "channel_id": self.current_channel_id,  # Also inform client of current channel
            }
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

                # Update all autonomous agents (BOT-QUEST-PROVIDERs)
                for obj_id, obj in list(
                    self.offline_network_state.get_all_network_objects().items()
                ):
                    if obj.network_object_type == "BOT-QUEST-PROVIDER":
                        obj.update_autonomous_movement(
                            offline_network_state=self.offline_network_state,
                            current_time=current_time,
                            delta_time=delta_time,
                            send_to_client_callback=self._send_to_client,
                        )

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
