import logging
import threading
import time

from raylibpy import (
    MOUSE_BUTTON_LEFT,
    RAYWHITE,
    Vector2,
    Color,
    is_window_resized,
    get_screen_width,
    get_screen_height,
)

from config import (
    CAMERA_SMOOTHNESS,
    NETWORK_OBJECT_SIZE,
    SCREEN_HEIGHT,
    SCREEN_WIDTH,
    WORLD_HEIGHT,
    WORLD_WIDTH,
    UI_MODAL_WIDTH,
    UI_MODAL_HEIGHT,
    UI_MODAL_PADDING_TOP,
    UI_MODAL_PADDING_RIGHT,
    UI_MODAL_BACKGROUND_COLOR,
    UI_TEXT_COLOR_PRIMARY,
    UI_TEXT_COLOR_SHADING,
    UI_FONT_SIZE,
)
from object_layer.object_layer_render import ObjectLayerRender
from network_state.network_object import NetworkObject
from network_state.network_state import NetworkState
from network_state.network_object_factory import NetworkObjectFactory
from network_state.network_state_proxy import NetworkStateProxy
from network_state.astar import astar
from ui.modal import Modal
from object_layer.camera_manager import CameraManager  # Import the new CameraManager


logging.basicConfig(
    level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s"
)


class InteractionManager:
    """
    Manages interactions between game objects, specifically detecting when a
    BOT-QUEST-PROVIDER intersects the player's ACTION_AREA.
    """

    def __init__(self, network_object_size: int):
        self.network_object_size = network_object_size
        # Scale for ACTION_AREA, matching the rendering scale in ObjectLayerRender
        self.action_area_scale = 2.5

    def _get_bounding_box(
        self, obj: NetworkObject, is_action_area: bool = False
    ) -> tuple[float, float, float, float]:
        """
        Calculates the bounding box (x, y, width, height) for a given network object.
        Adjusts size and position if it's the player's ACTION_AREA.
        """
        obj_x = obj.x
        obj_y = obj.y
        obj_size = self.network_object_size

        if is_action_area:
            scaled_size = obj_size * self.action_area_scale
            # Calculate offset to center the scaled action area
            offset = (scaled_size - obj_size) / 2
            obj_x -= offset
            obj_y -= offset
            obj_size = scaled_size

        return obj_x, obj_y, obj_size, obj_size

    def check_for_bot_interaction(
        self, network_state: NetworkState, my_player_id: str
    ) -> bool:
        """
        Checks if any BOT-QUEST-PROVIDER object is intersecting the main player's
        ACTION_AREA.

        Args:
            network_state: The current NetworkState instance.
            my_player_id: The ID of the controlling client's player.

        Returns:
            True if an intersection is detected, False otherwise.
        """
        with network_state.lock:
            player_obj = network_state.get_network_object(my_player_id)
            if not player_obj or "ACTION_AREA" not in player_obj.object_layer_ids:
                return False  # Player or ACTION_AREA not found

            # Get player's ACTION_AREA bounding box
            player_bb_x, player_bb_y, player_bb_width, player_bb_height = (
                self._get_bounding_box(player_obj, is_action_area=True)
            )

            for obj_id, obj in network_state.get_all_network_objects().items():
                if obj.network_object_type == "BOT-QUEST-PROVIDER":
                    # Get bot's bounding box (standard size)
                    bot_bb_x, bot_bb_y, bot_bb_width, bot_bb_height = (
                        self._get_bounding_box(obj, is_action_area=False)
                    )

                    # Check for intersection using AABB collision detection
                    # Two rectangles intersect if their x-intervals and y-intervals overlap
                    x_overlap = max(
                        0,
                        min(player_bb_x + player_bb_width, bot_bb_x + bot_bb_width)
                        - max(player_bb_x, bot_bb_x),
                    )
                    y_overlap = max(
                        0,
                        min(player_bb_y + player_bb_height, bot_bb_y + bot_bb_height)
                        - max(player_bb_y, bot_bb_y),
                    )

                    if x_overlap > 0 and y_overlap > 0:
                        logging.debug(
                            f"Intersection detected between player ACTION_AREA and BOT-QUEST-PROVIDER {obj_id}"
                        )
                        return True
        return False


class NetworkStateClient:
    """Manages the client-side application, rendering the world and handling user input."""

    def __init__(self, host: str, port: int, ws_path: str):
        self.host = host
        self.port = port
        self.ws_path = ws_path

        self.network_object_factory = NetworkObjectFactory()

        # Initialize CameraManager first
        self.camera_manager = CameraManager(SCREEN_WIDTH, SCREEN_HEIGHT)

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
        )

        # Initialize the UI modal for displaying information
        self.ui_modal = Modal(
            screen_width=SCREEN_WIDTH,
            screen_height=SCREEN_HEIGHT,
            render_content_callback=self._render_modal_content,
            mode="fixed",
            width=UI_MODAL_WIDTH,
            height=UI_MODAL_HEIGHT,
            padding_top=UI_MODAL_PADDING_TOP,
            padding_right=UI_MODAL_PADDING_RIGHT,
            background_color=Color(*UI_MODAL_BACKGROUND_COLOR),
        )
        self.show_modal = False  # Control modal visibility

        # Initialize the InteractionManager
        self.interaction_manager = InteractionManager(NETWORK_OBJECT_SIZE)

    def _set_my_player_id(self, player_id: str):
        """Sets the client's player ID, called by the proxy."""
        self.my_player_id = player_id

    def _process_queued_messages(self):
        """Processes messages received from the proxy."""
        with self.message_queue_lock:
            messages_to_process = list(self.message_queue)
            self.message_queue.clear()

        for data in messages_to_process:
            self._handle_proxy_message(data)

    def _handle_proxy_message(self, data: dict):
        """Handles messages forwarded by the proxy."""
        msg_type = data.get("type")
        if msg_type == "network_state_update":
            if "network_objects" in data:
                # The proxy now sends all persistent objects, including autonomous agents,
                # so we don't need to filter by is_persistent here in the client's update
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
                    self.my_network_object = self.network_state.network_objects.get(
                        self.my_player_id
                    )
            else:
                logging.error(
                    f"Received 'network_state_update' without 'network_objects': {data}"
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
                    f"Removed object {obj_id} with layer {object_layer_id} from rendering."
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
        """Sends a message to the NetworkStateProxy."""
        self.proxy.send_client_message(msg_type, payload)

    def _generate_client_path_gfx_async(
        self, path_coords: list[dict[str, float]], current_time: float
    ):
        """Generates POINT_PATH network objects sequentially for client-side path visualization."""
        obj_ids_to_remove = [
            obj_id
            for obj_id, obj in self.network_state.network_objects.items()
            if obj.network_object_type == "POINT_PATH" and not obj.is_persistent
        ]
        with self.network_state.lock:
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

    def _render_modal_content(
        self,
        object_layer_render_instance: ObjectLayerRender,
        x: int,
        y: int,
        width: int,
        height: int,
    ):
        """
        Renders content specific to the modal.
        This method is passed as a callback to the Modal.
        """
        # Example content for the modal (can be customized)
        modal_text = "Quest Available!"
        text_width = object_layer_render_instance.measure_text(modal_text, UI_FONT_SIZE)

        # Center the text within the modal
        text_x = x + (width - text_width) // 2
        text_y = y + (height - UI_FONT_SIZE) // 2

        object_layer_render_instance.draw_text(
            modal_text,
            text_x + 1,
            text_y + 1,
            UI_FONT_SIZE,
            Color(*UI_TEXT_COLOR_SHADING),
        )
        object_layer_render_instance.draw_text(
            modal_text,
            text_x,
            text_y,
            UI_FONT_SIZE,
            Color(*UI_TEXT_COLOR_PRIMARY),
        )

    def run(self):
        """Runs the main client loop."""
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

            # Handle window resize
            if is_window_resized():  # Changed from IsWindowResized
                self.camera_manager.handle_window_resize()
                # Update modal's screen dimensions as well
                self.ui_modal.screen_width = (
                    get_screen_width()
                )  # Changed from GetScreenWidth
                self.ui_modal.screen_height = (
                    get_screen_height()
                )  # Changed from GetScreenHeight
                self.ui_modal._configure_mode()  # Re-configure modal position

            self._process_queued_messages()

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

            # Check for BOT-QUEST-PROVIDER interaction and control modal visibility
            self.show_modal = self.interaction_manager.check_for_bot_interaction(
                self.network_state, self.my_player_id
            )

            if self.object_layer_render.is_mouse_button_pressed(MOUSE_BUTTON_LEFT):
                # Pass camera from camera_manager to get_world_mouse_position
                world_mouse_pos = self.object_layer_render.get_world_mouse_position(
                    self.camera_manager.camera
                )

                click_pointer_obj = self.network_object_factory.generate_click_pointer(
                    world_mouse_pos.x, world_mouse_pos.y, current_time
                )
                with self.network_state.lock:
                    self.network_state.add_or_update_network_object(click_pointer_obj)

                grid_x, grid_y = self.network_state._world_to_grid_coords(
                    world_mouse_pos.x, world_mouse_pos.y
                )

                is_valid_click = True
                if not (
                    0 <= grid_x < self.network_state.grid_cells_x
                    and 0 <= grid_y < self.network_state.grid_cells_y
                ):
                    logging.warning(
                        f"Click outside map boundaries: ({grid_x}, {grid_y})"
                    )
                    is_valid_click = False
                elif self.network_state.simplified_maze[grid_y][grid_x] == 1:
                    logging.warning(
                        f"Click on an obstacle at grid: ({grid_x}, {grid_y})"
                    )
                    is_valid_click = False

                if is_valid_click:
                    logging.info(
                        f"Mouse clicked at world position: ({world_mouse_pos.x}, {world_mouse_pos.y})"
                    )
                    move_request_payload = {
                        "target_x": world_mouse_pos.x,
                        "target_y": world_mouse_pos.y,
                    }
                    self.send_message_to_proxy(
                        "client_move_request", move_request_payload
                    )

                    if self.my_network_object:
                        start_grid_x, start_grid_y = (
                            self.network_state._world_to_grid_coords(
                                self.my_network_object.x, self.my_network_object.y
                            )
                        )
                        end_grid_x, end_grid_y = (
                            self.network_state._world_to_grid_coords(
                                world_mouse_pos.x, world_mouse_pos.y
                            )
                        )
                        maze = self.network_state.simplified_maze
                        path_grid = astar(
                            maze, (start_grid_y, start_grid_x), (end_grid_y, end_grid_x)
                        )

                        if path_grid:
                            path_world_coords = []
                            for grid_y, grid_x in path_grid:
                                world_x, world_y = (
                                    self.network_state._grid_to_world_coords(
                                        grid_x, grid_y
                                    )
                                )
                                path_world_coords.append({"X": world_x, "Y": world_y})

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
            # Pass camera from camera_manager to begin_camera_mode
            self.object_layer_render.begin_camera_mode(self.camera_manager.camera)

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
                    smoothed_player_pos = (
                        self.object_layer_render.get_smoothed_object_position(
                            self.my_player_id
                        )
                    )
                    camera_target_pos = Vector2(
                        (
                            smoothed_player_pos.x
                            if smoothed_player_pos
                            else self.my_network_object.x
                        )
                        + NETWORK_OBJECT_SIZE / 2,
                        (
                            smoothed_player_pos.y
                            if smoothed_player_pos
                            else self.my_network_object.y
                        )
                        + NETWORK_OBJECT_SIZE / 2,
                    )
                    # Use camera_manager to update camera target
                    self.camera_manager.update_camera_target(
                        camera_target_pos, smoothness=CAMERA_SMOOTHNESS
                    )

                    # Draw My Pos at top-left
                    pos_text = f" {int(self.my_network_object.x)}, {int(self.my_network_object.y)}"
                    font_size = UI_FONT_SIZE
                    padding = 10

                    # Draw shading first for My Pos
                    self.object_layer_render.draw_text(
                        pos_text,
                        padding + 1,
                        padding + 1,
                        font_size,
                        Color(*UI_TEXT_COLOR_SHADING),
                    )
                    # Draw actual text for My Pos
                    self.object_layer_render.draw_text(
                        pos_text,
                        padding,
                        padding,
                        font_size,
                        Color(*UI_TEXT_COLOR_PRIMARY),
                    )
                else:
                    # Draw "Connecting..." at top-left
                    connecting_text = "Connecting..."
                    font_size = UI_FONT_SIZE
                    padding = 10
                    self.object_layer_render.draw_text(
                        connecting_text,
                        padding + 1,
                        padding + 1,
                        font_size,
                        Color(*UI_TEXT_COLOR_SHADING),
                    )
                    self.object_layer_render.draw_text(
                        connecting_text,
                        padding,
                        padding,
                        font_size,
                        Color(*UI_TEXT_COLOR_PRIMARY),
                    )

            # Conditionally render the UI modal based on interaction
            if self.show_modal:
                self.ui_modal.render(self.object_layer_render)

            # Draw FPS at bottom-left
            frame_time = self.object_layer_render.get_frame_time()
            fps_text = f"FPS: {int(1.0 / frame_time) if frame_time > 0 else 'N/A'}"
            font_size = UI_FONT_SIZE - 3
            padding = 10
            # Draw shading first
            self.object_layer_render.draw_text(
                fps_text,
                padding + 1,
                SCREEN_HEIGHT - 30 + 1,
                font_size,
                Color(*UI_TEXT_COLOR_SHADING),
            )
            # Draw actual text
            self.object_layer_render.draw_text(
                fps_text,
                padding,
                SCREEN_HEIGHT - 30,
                font_size,
                Color(*UI_TEXT_COLOR_PRIMARY),
            )

            self.object_layer_render.update_all_active_object_layer_animations(
                delta_time, current_time
            )

            self.object_layer_render.end_drawing()

            time.sleep(0.001)

        self.proxy.close()
        self.object_layer_render.close_window()
        logging.info("Client window closed.")
