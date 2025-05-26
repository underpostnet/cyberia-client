import logging
import time
import threading
import json

# Import the pure WebSocket client library
import websocket  # pip install websocket-client

from raylibpy import (
    Color,
    Vector2,
    MOUSE_BUTTON_LEFT,
    BLACK,
    RAYWHITE,  # Assuming RAYWHITE is a common background color
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
)

# Import the new centralized Raylib manager
from core.raylib_manager import RaylibManager

# Import the refactored game renderer
from visuals.game_renderer import GameRenderer
from visuals.animation_manager import (
    AnimationManager,
)  # This import is still needed for type hinting/reference
from visuals.animation_manager import (
    Direction,
)  # Import Direction for initial object creation

# Import the refactored game state and game object
from core.game_state import GameState
from core.game_object import GameObject


class MmoClient:
    """
    The client component of the MMO, connecting to the Go server via pure WebSocket,
    managing the instance world state, rendering the world, and sending player actions.
    This class orchestrates communication, state updates, and rendering.
    """

    def __init__(self, host: str, port: int, ws_path: str):
        """
        Initializes the MmoClient.

        Args:
            host (str): The host address of the WebSocket server.
            port (int): The port of the WebSocket server.
            ws_path (str): The WebSocket path on the server.
        """
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

        # Initialize the centralized Raylib manager
        self.raylib_manager = RaylibManager(
            screen_width=SCREEN_WIDTH,
            screen_height=SCREEN_HEIGHT,
            title="Python MMO Client",
            target_fps=60,
        )

        # Initialize the game state, which holds all game objects
        self.game_state = GameState(
            WORLD_WIDTH, WORLD_HEIGHT, OBJECT_SIZE
        )  # Local copy of instance state
        self.my_player_id: str | None = None
        self.my_game_object: GameObject | None = (
            None  # Reference to my player object in game_state.objects
        )
        self.current_path_display: list[dict[str, float]] = (
            []
        )  # Path received from server to display

        # Initialize the game renderer, passing the Raylib manager to it
        self.game_renderer = GameRenderer(
            raylib_manager=self.raylib_manager,
            screen_width=SCREEN_WIDTH,
            screen_height=SCREEN_HEIGHT,
            world_width=WORLD_WIDTH,
            world_height=WORLD_HEIGHT,
            object_size=OBJECT_SIZE,
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
        self.my_game_object = None
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
            # Before updating game_state, remove animations for objects that no longer exist
            # This handles object despawn/disconnection cleanly.
            current_object_ids = set(self.game_state.objects.keys())
            new_object_ids = set(data["objects"].keys())

            objects_to_remove = current_object_ids - new_object_ids
            for obj_id in objects_to_remove:
                # Remove all animations associated with this object ID
                self.game_renderer.animation_manager.remove_animation(obj_id)

            self.game_state.from_dict(data)  # This will create/update GameObjects

            if self.my_player_id and self.my_player_id in self.game_state.objects:
                self.my_game_object = self.game_state.objects[self.my_player_id]
            else:
                self.my_game_object = None
            logging.debug("Instance state updated.")
        elif msg_type == "player_assigned":
            self.my_player_id = data["player_id"]
            logging.info(f"Assigned player ID: {self.my_player_id} from server.")
        elif msg_type == "player_path_update":
            player_id = data["player_id"]
            path = data["path"]
            if player_id == self.my_player_id:
                self.current_path_display = path
                if self.my_game_object:
                    self.my_game_object.path = path
                    self.my_game_object.path_index = 0
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
            self.raylib_manager.close_window()  # Use raylib_manager to close window
            return

        last_frame_time = time.time()

        while (
            not self.raylib_manager.window_should_close()
        ):  # Use raylib_manager for window close check
            current_time = time.time()
            delta_time = current_time - last_frame_time
            last_frame_time = current_time

            # Process any messages received from the WebSocket thread
            self._process_queued_messages()

            # Update client-side object positions and get movement deltas
            # The direction smoothing logic is now handled by AnimationManager
            object_movement_deltas = {}  # Store (dx, dy) for each object
            for obj_id, obj in self.game_state.objects.items():
                dx, dy = obj.update_position(delta_time)
                object_movement_deltas[obj_id] = (dx, dy)

                # If the object reached the end of its path, clear it
                if obj.path and obj.path_index >= len(obj.path):
                    obj.path = []
                    obj.path_index = 0
                    # If it's my player, also clear the current_path_display
                    if obj_id == self.my_player_id:
                        self.current_path_display = []

            # --- Input Handling ---
            if self.raylib_manager.is_mouse_button_pressed(
                MOUSE_BUTTON_LEFT
            ):  # Use raylib_manager for input
                world_mouse_pos = (
                    self.raylib_manager.get_world_mouse_position()
                )  # Use raylib_manager for world mouse pos
                logging.info(
                    f"Mouse clicked at world position: ({world_mouse_pos.x}, {world_mouse_pos.y})"
                )
                # The send_message function now handles the player_id check
                self.send_message(
                    "client_move_request",
                    {"target_x": world_mouse_pos.x, "target_y": world_mouse_pos.y},
                )

            # --- Rendering ---
            self.raylib_manager.begin_drawing()  # Use raylib_manager to begin drawing
            self.raylib_manager.clear_background(
                RAYWHITE
            )  # Use raylib_manager to clear background
            self.raylib_manager.begin_camera_mode()  # Use raylib_manager to begin camera mode

            self.game_renderer.draw_grid()  # Delegate to game_renderer

            # Draw all objects received from the server
            for obj_id, obj in self.game_state.objects.items():
                # Pass the movement deltas to the game renderer
                dx, dy = object_movement_deltas.get(obj_id, (0.0, 0.0))
                self.game_renderer.draw_game_object(obj, current_time, dx, dy)

            # Draw the path for my player (if any)
            if self.my_game_object and self.current_path_display:
                self.game_renderer.draw_path(
                    self.current_path_display
                )  # Delegate to game_renderer

            self.raylib_manager.end_camera_mode()  # Use raylib_manager to end camera mode

            # Update camera to follow my player
            if self.my_game_object:
                self.raylib_manager.update_camera_target(  # Use raylib_manager to update camera
                    Vector2(
                        self.my_game_object.x + OBJECT_SIZE / 2,
                        self.my_game_object.y + OBJECT_SIZE / 2,
                    ),
                    smoothness=0.1,  # CAMERA_SMOOTHNESS from config, or a default value
                )
                # Display my player ID
                self.raylib_manager.draw_text(  # Use raylib_manager to draw text
                    f"My Player ID: {self.my_player_id}", 10, 10, 20, BLACK
                )
                self.raylib_manager.draw_text(  # Use raylib_manager to draw text
                    f"My Pos: ({int(self.my_game_object.x)}, {int(self.my_game_object.y)})",
                    10,
                    40,
                    20,
                    BLACK,
                )
            else:
                self.raylib_manager.draw_text(
                    "Connecting...", 10, 10, 20, BLACK
                )  # Use raylib_manager to draw text

            # Display FPS - Ensure get_frame_time() is not zero
            frame_time = (
                self.raylib_manager.get_frame_time()
            )  # Use raylib_manager for frame time
            if frame_time > 0:
                self.raylib_manager.draw_text(  # Use raylib_manager to draw text
                    f"FPS: {int(1.0 / frame_time)}", 10, SCREEN_HEIGHT - 30, 20, BLACK
                )
            else:
                self.raylib_manager.draw_text(  # Use raylib_manager to draw text
                    "FPS: N/A", 10, SCREEN_HEIGHT - 30, 20, BLACK
                )  # Display N/A if frame_time is 0

            # Update all active animations managed by GameRenderer's AnimationManager
            self.game_renderer.animation_manager.update_all_active_animations(
                delta_time, current_time
            )

            self.raylib_manager.end_drawing()  # Use raylib_manager to end drawing

            # Add a small sleep to yield control, allowing Raylib to process events
            time.sleep(0.001)

        # Clean up WebSocket connection on window close
        if self.ws:
            self.ws.close()
        logging.info("WebSocket connection closed.")
        self.raylib_manager.close_window()  # Use raylib_manager to close window
        logging.info("Client window closed.")
