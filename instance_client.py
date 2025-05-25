import logging
import time
import threading
import json

# Import the pure WebSocket client library
import websocket  # pip install websocket-client

from raylibpy import (
    Color,
    Vector2,
    Camera2D,
    RAYWHITE,
    DARKGRAY,
    LIGHTGRAY,
    BLACK,
    BLUE,
    RED,
    GREEN,
    MOUSE_BUTTON_LEFT,
    init_window,
    set_target_fps,
    begin_drawing,
    clear_background,
    begin_mode2d,
    draw_line,
    draw_rectangle,
    draw_circle,
    draw_text,
    end_mode2d,
    get_frame_time,
    get_mouse_position,
    get_screen_to_world2d,
    close_window,
    window_should_close,
    is_mouse_button_pressed,
    end_drawing,
)

# --- Logging Configuration ---
logging.basicConfig(
    level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s"
)

from config import (
    SCREEN_WIDTH,
    SCREEN_HEIGHT,
    WORLD_WIDTH,
    WORLD_HEIGHT,
    OBJECT_SIZE,
    WORLD_WIDTH,
)

from instance_renderer import InstanceRenderer
from instance_state import InstanceState
from instance_object import InstanceObject


class InstanceClient:
    """
    The client component of the MMO, connecting to the Go server via pure WebSocket,
    rendering the instance world, and sending player actions.
    """

    def __init__(self, host: str, port: int, ws_path: str):
        self.host = host
        self.port = port
        self.ws_path = ws_path
        self.websocket_url = f"ws://{self.host}:{self.port}{self.ws_path}"
        self.ws: websocket.WebSocketApp | None = (
            None  # Will hold the websocket-client instance
        )
        self.ws_thread: threading.Thread | None = (
            None  # Thread for WebSocket communication
        )

        self.instance_state = InstanceState(
            WORLD_WIDTH, WORLD_HEIGHT, OBJECT_SIZE
        )  # Local copy of instance state
        self.my_player_id: str | None = None
        self.my_instance_obj: InstanceObject | None = (
            None  # Reference to my player object in instance_state.objects
        )
        self.current_path_display: list[dict[str, float]] = (
            []
        )  # Path received from server to display

        self.renderer = InstanceRenderer(
            SCREEN_WIDTH, SCREEN_HEIGHT, WORLD_WIDTH, WORLD_HEIGHT, OBJECT_SIZE
        )

        self.message_queue: list[dict] = []  # Thread-safe queue for incoming messages
        self.message_queue_lock = threading.Lock()
        self.connection_ready_event = (
            threading.Event()
        )  # Event to signal connection readiness

    def _on_message(self, ws: websocket.WebSocketApp, message: str):
        """Handler for incoming WebSocket messages."""
        try:
            data = json.loads(message)
            with self.message_queue_lock:
                self.message_queue.append(data)
        except json.JSONDecodeError:
            logging.error(f"Failed to decode JSON message: {message}")
        except Exception as e:
            logging.exception(f"Error in on_message: {e}")

    def _on_error(self, ws: websocket.WebSocketApp, error: Exception):
        """Handler for WebSocket errors."""
        logging.error(f"WebSocket error: {error}")
        self.connection_ready_event.clear()  # Clear event on error

    def _on_close(
        self, ws: websocket.WebSocketApp, close_status_code: int, close_msg: str
    ):
        """Handler for WebSocket close event."""
        logging.info(f"Disconnected from server: {close_status_code} - {close_msg}")
        self.my_player_id = None
        self.my_instance_obj = None
        self.ws = None  # Mark connection as closed
        self.connection_ready_event.clear()  # Clear event on close

    def _on_open(self, ws: websocket.WebSocketApp):
        """Handler for successful WebSocket connection."""
        logging.info("Connected to server via Pure WebSocket!")
        self.connection_ready_event.set()  # Set event to signal connection is ready

    def _process_queued_messages(self):
        """Processes messages from the queue in the main instance loop."""
        with self.message_queue_lock:
            messages_to_process = list(self.message_queue)
            self.message_queue.clear()

        for data in messages_to_process:
            self._handle_server_message(data)

    def _handle_server_message(self, data: dict):
        """Dispatches incoming server messages based on their 'type' field."""
        msg_type = data.get("type")
        if msg_type == "instance_state_update":
            self.instance_state.from_dict(data)
            if self.my_player_id and self.my_player_id in self.instance_state.objects:
                self.my_instance_obj = self.instance_state.objects[self.my_player_id]
            else:
                self.my_instance_obj = None
            logging.debug("Instance state updated.")
        elif msg_type == "player_assigned":
            self.my_player_id = data["player_id"]
            logging.info(f"Assigned player ID: {self.my_player_id} from server.")
        elif msg_type == "player_path_update":
            player_id = data["player_id"]
            path = data["path"]
            if player_id == self.my_player_id:
                self.current_path_display = path
                if self.my_instance_obj:
                    self.my_instance_obj.path = path
                    self.my_instance_obj.path_index = 0
                logging.info(f"Received new path for my player: {len(path)} steps.")
        elif msg_type == "message":
            logging.info(f"Server message: {data.get('text', 'No message text')}")
        else:
            logging.warning(f"Unknown message type received: {msg_type} - {data}")

    def send_message(self, msg_type: str, payload: dict | None = None):
        """Sends a JSON message over the WebSocket."""
        # Check if the WebSocket connection is active and player ID is assigned before sending
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

    def run(self):
        """Main client loop, handles rendering and user input."""
        logging.info(f"Connecting to WebSocket server at {self.websocket_url}")

        # Create WebSocketApp instance
        self.ws = websocket.WebSocketApp(
            self.websocket_url,
            on_open=self._on_open,
            on_message=self._on_message,
            on_error=self._on_error,
            on_close=self._on_close,
        )

        # Run the WebSocketApp in a separate thread
        self.ws_thread = threading.Thread(target=self.ws.run_forever, daemon=True)
        self.ws_thread.start()

        # Wait for the connection to be ready with a timeout
        if not self.connection_ready_event.wait(timeout=5):  # Wait up to 5 seconds
            logging.error("Failed to establish WebSocket connection within timeout.")
            self.renderer.close_window()
            return

        last_frame_time = time.time()

        while not self.renderer.window_should_close():
            current_frame_time = time.time()
            delta_time = current_frame_time - last_frame_time
            last_frame_time = current_frame_time

            # Process any messages received from the WebSocket thread
            self._process_queued_messages()

            # Update client-side player movement based on received path
            if self.my_instance_obj and self.my_instance_obj.path:
                self.my_instance_obj.update_position(delta_time)
                # If the client-side player reached the end of its path, clear it
                if self.my_instance_obj.path_index >= len(self.my_instance_obj.path):
                    self.my_instance_obj.path = []
                    self.my_instance_obj.path_index = 0
                    self.current_path_display = []  # Clear displayed path

            # --- Input Handling ---
            if is_mouse_button_pressed(MOUSE_BUTTON_LEFT):
                world_mouse_pos = self.renderer.get_world_mouse_position()
                logging.info(
                    f"Mouse clicked at world position: ({world_mouse_pos.x}, {world_mouse_pos.y})"
                )
                # The send_message function now handles the player_id check
                self.send_message(
                    "client_move_request",
                    {"target_x": world_mouse_pos.x, "target_y": world_mouse_pos.y},
                )

            # --- Rendering ---
            self.renderer.begin_frame()

            self.renderer.draw_grid()

            # Draw all objects received from the server
            for obj_id, obj in self.instance_state.objects.items():
                self.renderer.draw_object(obj)

            # Draw the path for my player
            if self.my_instance_obj and self.current_path_display:
                self.renderer.draw_path(self.current_path_display)

            # Update camera to follow my player
            if self.my_instance_obj:
                self.renderer.update_camera(
                    Vector2(
                        self.my_instance_obj.x + OBJECT_SIZE / 2,
                        self.my_instance_obj.y + OBJECT_SIZE / 2,
                    )
                )
                # Display my player ID
                self.renderer.draw_debug_info(
                    f"My Player ID: {self.my_player_id}", 10, 10, 20, BLACK
                )
                self.renderer.draw_debug_info(
                    f"My Pos: ({int(self.my_instance_obj.x)}, {int(self.my_instance_obj.y)})",
                    10,
                    40,
                    20,
                    BLACK,
                )
            else:
                self.renderer.draw_debug_info("Connecting...", 10, 10, 20, BLACK)

            # Display FPS - Ensure get_frame_time() is not zero
            frame_time = get_frame_time()
            if frame_time > 0:
                self.renderer.draw_debug_info(
                    f"FPS: {int(1.0 / frame_time)}", 10, SCREEN_HEIGHT - 30, 20, BLACK
                )
            else:
                self.renderer.draw_debug_info(
                    "FPS: N/A", 10, SCREEN_HEIGHT - 30, 20, BLACK
                )  # Display N/A if frame_time is 0

            self.renderer.end_frame()

            # Add a small sleep to yield control, allowing Raylib to process events
            time.sleep(0.001)

        # Clean up WebSocket connection on window close
        if self.ws:
            self.ws.close()
        logging.info("WebSocket connection closed.")
        self.renderer.close_window()
        logging.info("Client window closed.")
