import logging
import threading
import time
from typing import Union
from functools import partial

from pyray import (
    MOUSE_BUTTON_LEFT,
    RAYWHITE,
    Vector2,
    Color,
    is_window_resized,
    get_screen_width,
    get_screen_height,
    get_mouse_position,
    get_mouse_wheel_move,
    is_mouse_button_down,  # Import is_mouse_button_down
)

from config import (
    CAMERA_SMOOTHNESS,
    UI_FONT_SIZE,
    UI_TEXT_COLOR_PRIMARY,
    UI_TEXT_COLOR_SHADING,
    SCREEN_HEIGHT,
    SCREEN_WIDTH,
    WORLD_HEIGHT,
    WORLD_WIDTH,
    NETWORK_OBJECT_SIZE,
    UI_MODAL_BACKGROUND_COLOR,
)
from object_layer.object_layer_render import ObjectLayerRender
from network_state.network_object import NetworkObject
from network_state.network_state import NetworkState
from network_state.network_object_factory import NetworkObjectFactory
from network_state.astar import astar
from ui.components.core.modal_core_component import ModalCoreComponent
from object_layer.camera_manager import CameraManager

# Import UI Views
from ui.views.cyberia.bag_cyberia_view import BagCyberiaView
from ui.views.cyberia.quest_cyberia_view import QuestCyberiaView
from ui.views.cyberia.character_cyberia_view import CharacterCyberiaView
from ui.views.cyberia.chat_cyberia_view import ChatCyberiaView
from ui.views.cyberia.map_cyberia_view import MapCyberiaView

# Import rendering utilities (these are passed as callbacks, not directly managed here)
from ui.components.cyberia.modal_render_cyberia import (
    render_modal_action_area_discovery_content,
    render_modal_bag_view_content,
    render_modal_close_btn_content,
    render_modal_btn_icon_content,
    render_modal_character_view_content,
    render_modal_chat_view_content,
    render_modal_map_view_content,
    render_modal_quest_list_content,
)

# Import new core components
from ui.components.core.router_core_component import RouterCoreComponent
from ui.components.core.texture_manager import TextureManager
from ui.components.core.keyboard_core_component import KeyboardCoreComponent
from network_state.network_state_proxy import NetworkStateProxy


logging.basicConfig(
    level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s"
)


# UI Route Definitions (for RouterCoreComponent)
# Each route defines a UI panel/modal and its associated navigation icon.
# 'name': Display name for the navigation button.
# 'path': Unique identifier for the route.
# 'icon_path': Path to the icon image for the navigation button.
# 'view_instance': Placeholder for the actual view class instance (e.g., BagCyberiaView).
# 'render_callback': Reference to the function that renders the view's content.
UI_ROUTES = [
    {
        "name": "Character",
        "path": "/character",
        "icon_path": "ui/assets/icons/character.png",
        "view_instance": None,
        "render_callback": None,
    },
    {
        "name": "Bag",
        "path": "/bag",
        "icon_path": "ui/assets/icons/bag.png",
        "view_instance": None,
        "render_callback": None,
    },
    {
        "name": "Chat",
        "path": "/chat",
        "icon_path": "ui/assets/icons/chat.png",
        "view_instance": None,
        "render_callback": None,
    },
    {
        "name": "Quest",
        "path": "/quest",
        "icon_path": "ui/assets/icons/quest.png",
        "view_instance": None,
        "render_callback": None,
    },
    {
        "name": "Map",
        "path": "/map",
        "icon_path": "ui/assets/icons/map.png",
        "view_instance": None,
        "render_callback": None,
    },
]


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

        # Initialize TextureManager FIRST
        self.texture_manager = TextureManager()

        # Initialize CameraManager
        self.camera_manager = CameraManager(SCREEN_WIDTH, SCREEN_HEIGHT)

        # Initialize KeyboardCoreComponent
        self.keyboard_core_component = KeyboardCoreComponent(
            backspace_initial_delay=0.4,
            backspace_repeat_rate=0.05,
        )

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
            WORLD_WIDTH, WORLD_HEIGHT, NETWORK_OBJECT_SIZE  # type: ignore
        )  # type: ignore
        self.my_player_id: Union[str, None] = None  # type: ignore
        self.my_network_object: Union[NetworkObject, None] = None  # type: ignore

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

        # Initialize the single large modal for action area interaction (top-right)
        self.modal_action_area_discovery = (
            ModalCoreComponent(  # Renamed from modal_quest_discovery
                screen_width=SCREEN_WIDTH,
                screen_height=SCREEN_HEIGHT,
                render_content_callback=partial(
                    render_modal_action_area_discovery_content
                ),  # Updated callback
                width=280,
                height=80,
                padding_bottom=SCREEN_HEIGHT - 5 - 80,
                padding_right=5,
                horizontal_offset=0,
                background_color=Color(*UI_MODAL_BACKGROUND_COLOR),
            )
        )
        self.show_modal_action_area_discovery = False  # Renamed flag

        # Initialize UI Views (these will be passed to the router)
        self.bag_cyberia_view = BagCyberiaView(
            object_layer_render_instance=self.object_layer_render
        )
        self.quest_cyberia_view = QuestCyberiaView(
            object_layer_render_instance=self.object_layer_render
        )
        self.character_cyberia_view = CharacterCyberiaView(
            object_layer_render_instance=self.object_layer_render
        )
        self.chat_cyberia_view = ChatCyberiaView(
            object_layer_render_instance=self.object_layer_render,
            network_proxy=self.proxy,  # Pass the proxy instance
        )
        self.map_cyberia_view = MapCyberiaView(
            object_layer_render_instance=self.object_layer_render,
            network_proxy=self.proxy,  # Pass the proxy instance
        )

        # Populate UI_ROUTES with view instances and render callbacks
        for route in UI_ROUTES:
            if route["path"] == "/character":
                route["view_instance"] = self.character_cyberia_view
                route["render_callback"] = render_modal_character_view_content
            elif route["path"] == "/bag":
                route["view_instance"] = self.bag_cyberia_view
                route["render_callback"] = render_modal_bag_view_content
            elif route["path"] == "/chat":
                route["view_instance"] = self.chat_cyberia_view
                route["render_callback"] = render_modal_chat_view_content
            elif route["path"] == "/quest":
                route["view_instance"] = self.quest_cyberia_view
                route["render_callback"] = render_modal_quest_list_content
            elif route["path"] == "/map":
                route["view_instance"] = self.map_cyberia_view
                route["render_callback"] = render_modal_map_view_content

        # Initialize RouterCoreComponent
        self.router = RouterCoreComponent(
            screen_width=SCREEN_WIDTH,
            screen_height=SCREEN_HEIGHT,
            object_layer_render_instance=self.object_layer_render,
            texture_manager=self.texture_manager,
            keyboard_core_component=self.keyboard_core_component,
            network_proxy=self.proxy,  # Pass the proxy instance
            routes=UI_ROUTES,
            ui_modal_background_color=Color(*UI_MODAL_BACKGROUND_COLOR),
            # Pass rendering callbacks for generic button content
            render_modal_btn_icon_content_callback=render_modal_btn_icon_content,
            render_modal_close_btn_content_callback=render_modal_close_btn_content,
        )

        # Initialize the InteractionManager
        self.interaction_manager = InteractionManager(NETWORK_OBJECT_SIZE)

    def _set_my_player_id(self, player_id: str):  # type: ignore
        """Sets the client's player ID, called by the proxy."""  # type: ignore
        self.my_player_id = player_id

    def _process_queued_messages(self):
        """Processes messages received from the proxy."""
        with self.message_queue_lock:
            messages_to_process = list(self.message_queue)
            self.message_queue.clear()

        for data in messages_to_process:
            self._handle_proxy_message(data)

    def _handle_proxy_message(self, data: dict):  # type: ignore
        """Handles messages forwarded by the proxy."""  # type: ignore
        msg_type = data.get("type")
        if msg_type == "network_state_update":
            if "clean_background_color" in data:
                self.network_state.clean_background_color = data[
                    "clean_background_color"
                ]

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
        elif msg_type == "server_chat_message":
            # Ensure chat_cyberia_view is initialized and the message is valid
            if self.chat_cyberia_view and data.get("data"):
                # Pass player ID from client.go to chat_cyberia_view
                data["data"]["player_id"] = self.my_player_id
                self.chat_cyberia_view.handle_server_chat_message(data)
            else:
                logging.warning(
                    f"Could not handle server_chat_message: view not ready or message malformed: {data}"
                )

        else:
            logging.warning(
                f"Unknown message type received from proxy: {msg_type} - {data}"
            )

    def send_message_to_proxy(self, msg_type: str, payload: Union[dict, None] = None):  # type: ignore
        """Sends a message to the NetworkStateProxy."""
        self.proxy.send_client_message(msg_type, payload)

    # type: ignore
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
            # Correctly generate POINT_PATH objects instead of CLICK_POINTER
            obj = self.network_object_factory.generate_point_path(
                [point], current_time
            )[0]
            obj.is_persistent = False  # Ensure path points decay
            obj.decay_time = current_time + 1.0  # Decay after 1 second
            with self.network_state.lock:
                self.network_state.add_or_update_network_object(obj)
            time.sleep(delay_per_point)
        logging.debug(
            f"Generated {len(path_coords)} client-side path GFX points asynchronously."
        )

    # type: ignore
    def run(self):
        """Runs the main client loop."""
        self.proxy.connect()

        if not self.connection_ready_event.wait(timeout=10):
            logging.error("Failed to establish connection via proxy within timeout.")
            self.texture_manager.unload_all_textures()  # Unload textures on exit
            self.object_layer_render.close_window()
            self.proxy.close()
            return

        last_frame_time = time.time()

        while not self.object_layer_render.window_should_close():
            current_time = time.time()
            delta_time = current_time - last_frame_time
            last_frame_time = current_time

            # Update keyboard state at the beginning of the frame
            self.keyboard_core_component.update(delta_time)

            mouse_pos = get_mouse_position()
            mouse_x, mouse_y = int(mouse_pos.x), int(mouse_pos.y)
            is_mouse_left_button_pressed = (
                self.object_layer_render.is_mouse_button_pressed(MOUSE_BUTTON_LEFT)
            )
            is_mouse_left_button_down = is_mouse_button_down(
                MOUSE_BUTTON_LEFT
            )  # Get mouse button down state

            # Handle window resize
            if is_window_resized():
                self.camera_manager.handle_window_resize()
                new_screen_width = get_screen_width()
                new_screen_height = get_screen_height()
                # Update router and action area discovery modal dimensions and position
                self.router.update_screen_dimensions(
                    new_screen_width, new_screen_height
                )
                self.modal_action_area_discovery.screen_width = new_screen_width
                self.modal_action_area_discovery.screen_height = new_screen_height
                self.modal_action_area_discovery.x = new_screen_width - 280 - 5
                self.modal_action_area_discovery.y = (
                    new_screen_height - (new_screen_height - 5 - 80) - 80
                )

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

            # Check for BOT-QUEST-PROVIDER interaction and control action area discovery modal visibility
            # modal_action_area_discovery should not be activated if any right panel is active
            right_panel_active = self.router.active_route_path is not None
            if not right_panel_active:
                self.show_modal_action_area_discovery = (
                    self.interaction_manager.check_for_bot_interaction(
                        self.network_state, self.my_player_id
                    )
                )
            else:
                self.show_modal_action_area_discovery = False

            # Handle clicks for UI buttons (delegated to router)
            modal_was_clicked_this_frame = False
            # Check for mouse wheel movement and pass it to the router's active view
            # This is specifically for the map view's zoom
            mouse_wheel_move = get_mouse_wheel_move()
            if mouse_wheel_move != 0:
                # If there's an active view and it's the map, let it handle the scroll
                if self.router.active_view_modal:
                    current_route_data = next(
                        (
                            r
                            for r in self.router.routes
                            if r["path"] == self.router.active_route_path
                        ),
                        None,
                    )
                    if current_route_data and current_route_data["path"] == "/map":
                        # The map view itself will call get_mouse_wheel_move() internally
                        # We just need to ensure the render method is called which then calls _handle_zoom_input
                        logging.debug(
                            "Mouse wheel scroll detected over active map view."
                        )

            # Pass is_mouse_left_button_down to handle_control_button_clicks and handle_navigation_button_clicks
            # This ensures they can correctly identify a "down" state for interactions
            if self.router.handle_control_button_clicks(
                mouse_x, mouse_y, is_mouse_left_button_pressed
            ):
                modal_was_clicked_this_frame = True
            elif self.router.handle_navigation_button_clicks(
                mouse_x, mouse_y, is_mouse_left_button_pressed
            ):
                modal_was_clicked_this_frame = True

            # If a right panel modal is active, allow it to handle its internal clicks
            # This also sets modal_was_clicked_this_frame if an internal element was clicked
            if self.router.active_view_modal:
                # This should be called *regardless* of `is_mouse_button_down`
                # because `handle_mouse_input` needs the `is_mouse_button_down` state for dragging.
                # However, `handle_view_clicks` typically checks `is_mouse_button_pressed` itself
                # for discrete clicks.
                if self.router.handle_view_clicks(
                    mouse_x,
                    mouse_y,
                    is_mouse_left_button_pressed,  # Pass specific press for clicks
                    # Removed is_mouse_left_button_down to match expected 4 arguments
                ):
                    modal_was_clicked_this_frame = True
                # If a view modal is active and a click occurred within its bounds,
                # even if no specific internal interactive element was clicked, consider it handled
                # by the UI to prevent world interaction.
                elif (
                    is_mouse_left_button_pressed  # Only if button was pressed this frame
                    and mouse_x >= self.router.active_view_modal.x
                    and mouse_x
                    <= (
                        self.router.active_view_modal.x
                        + self.router.active_view_modal.width
                    )
                    and mouse_y >= self.router.active_view_modal.y
                    and mouse_y
                    <= (
                        self.router.active_view_modal.y
                        + self.router.active_view_modal.height
                    )
                ):
                    logging.debug(
                        "Click on active view modal background, preventing world interaction."
                    )
                    modal_was_clicked_this_frame = True

            # Check action area discovery modal
            if self.show_modal_action_area_discovery:
                if self.modal_action_area_discovery.check_click(
                    mouse_x, mouse_y, is_mouse_left_button_pressed
                ):
                    modal_was_clicked_this_frame = True
                # If action area discovery modal is active and a click occurred within its bounds,
                # even if no specific internal interactive element was clicked, consider it handled
                # by the UI to prevent world interaction.
                elif (
                    is_mouse_left_button_pressed  # Only if button was pressed this frame
                    and mouse_x >= self.modal_action_area_discovery.x
                    and mouse_x
                    <= (
                        self.modal_action_area_discovery.x
                        + self.modal_action_area_discovery.width
                    )
                    and mouse_y >= self.modal_action_area_discovery.y
                    and mouse_y
                    <= (
                        self.modal_action_area_discovery.y
                        + self.modal_action_area_discovery.height
                    )
                ):
                    logging.debug(
                        "Click on action area discovery modal background, preventing world interaction."
                    )
                    modal_was_clicked_this_frame = True

            # Only process world clicks if no modal was clicked in this frame and mouse button was just pressed
            if not modal_was_clicked_this_frame and is_mouse_left_button_pressed:
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
            self.object_layer_render.clear_background(
                Color(
                    self.network_state.clean_background_color[0],
                    self.network_state.clean_background_color[1],
                    self.network_state.clean_background_color[2],
                    self.network_state.clean_background_color[3],
                )
            )
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

                    # Draw shading first
                    self.object_layer_render.draw_text(
                        pos_text,
                        padding + 1,
                        padding + 1,
                        font_size,
                        Color(*UI_TEXT_COLOR_SHADING),
                    )
                    # Draw actual text
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

            # Render modals AFTER world rendering to ensure they are on top
            if self.show_modal_action_area_discovery:
                # Pass mouse_x, mouse_y to enable hover effect logic in ModalCoreComponent
                self.modal_action_area_discovery.render(
                    self.object_layer_render,
                    mouse_x,
                    mouse_y,
                    is_mouse_left_button_down,  # Pass is_mouse_left_button_down
                )

            # Render UI through the router
            player_obj_layer_ids = []
            with self.network_state.lock:
                if self.my_network_object:
                    player_obj_layer_ids = self.my_network_object.object_layer_ids
            # Pass player_object_layer_ids, network_state, and my_network_object through the router's data_to_pass mechanism
            # Update the data_to_pass for router's current active view.
            # This is critical for views like BagCyberiaView that need player-specific data.
            if self.router.active_view_modal:
                current_route_data = next(
                    (
                        r
                        for r in self.router.routes
                        if r["path"] == self.router.active_route_path
                    ),
                    None,
                )
                if current_route_data:
                    self.router.active_view_modal.data_to_pass.update(
                        {
                            "player_object_layer_ids": player_obj_layer_ids,
                            "network_state": self.network_state,  # Pass network_state
                            "my_network_object": self.my_network_object,  # Pass my_network_object
                            "is_mouse_button_down": is_mouse_left_button_down,  # Ensure this is passed
                        }
                    )

            self.router.render(
                mouse_x, mouse_y, is_mouse_left_button_pressed, delta_time
            )

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

        self.texture_manager.unload_all_textures()  # Unload all textures managed by the TextureManager
        self.proxy.close()
        self.object_layer_render.close_window()
        logging.info("Client window closed.")
