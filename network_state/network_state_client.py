import logging
import threading
import time
from functools import partial

from raylibpy import (
    MOUSE_BUTTON_LEFT,
    RAYWHITE,
    Vector2,
    Color,
    is_window_resized,
    get_screen_width,
    get_screen_height,
    load_texture,
    unload_texture,
    get_mouse_position,
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
from network_state.network_state_proxy import NetworkStateProxy
from network_state.astar import astar
from ui.components.core.modal_core_component import ModalCoreComponent
from object_layer.camera_manager import CameraManager
from ui.views.cyberia.bag_cyberia_view import BagCyberiaView
from ui.views.cyberia.quest_cyberia_view import QuestCyberiaView
from ui.views.cyberia.character_cyberia_view import CharacterCyberiaView

# Import rendering utilities from the new path
from ui.components.cyberia.modal_render_cyberia import (
    render_modal_quest_discovery_content,
    render_modal_bag_view_content,
    render_modal_close_btn_content,
    render_modal_btn_icon_content,
    render_modal_character_view_content,
    render_modal_chat_view_content,
    render_modal_map_view_content,
    render_modal_quest_list_content,
)


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

        # Initialize the single large modal for quest interaction (top-right)
        self.modal_quest_discovery = ModalCoreComponent(
            screen_width=SCREEN_WIDTH,
            screen_height=SCREEN_HEIGHT,
            render_content_callback=partial(render_modal_quest_discovery_content),
            width=280,
            height=80,
            padding_bottom=SCREEN_HEIGHT - 5 - 80,
            padding_right=5,
            horizontal_offset=0,
            background_color=Color(*UI_MODAL_BACKGROUND_COLOR),
        )
        self.show_modal_quest_discovery = False

        # Initialize BagCyberiaView instance (stateful)
        self.bag_cyberia_view = BagCyberiaView(
            object_layer_render_instance=self.object_layer_render
        )

        # Initialize the large modal for the bag view (right side)
        self.modal_bag_view = ModalCoreComponent(
            screen_width=SCREEN_WIDTH,
            screen_height=SCREEN_HEIGHT,
            render_content_callback=partial(render_modal_bag_view_content),
            width=300,  # Fixed width
            height=SCREEN_HEIGHT,  # Same as screen height
            padding_bottom=0,
            padding_right=0,  # Fixed to the right edge
            horizontal_offset=0,
            background_color=Color(0, 0, 0, 200),  # Slightly darker for view
            title_text=self.bag_cyberia_view.title_text,  # Initial title
        )
        self.show_modal_bag_view = False  # State to control visibility

        # Initialize QuestCyberiaView instance (stateful)
        self.quest_cyberia_view = QuestCyberiaView(
            object_layer_render_instance=self.object_layer_render
        )

        # Initialize the large modal for the quest list view (right side, overlapping bag)
        self.modal_quest_list_view = ModalCoreComponent(
            screen_width=SCREEN_WIDTH,
            screen_height=SCREEN_HEIGHT,
            render_content_callback=partial(render_modal_quest_list_content),
            width=300,
            height=SCREEN_HEIGHT,
            padding_bottom=0,
            padding_right=0,
            horizontal_offset=0,
            background_color=Color(0, 0, 0, 200),
            title_text=self.quest_cyberia_view.title_text,
        )
        self.show_modal_quest_list_view = False

        # Initialize CharacterCyberiaView instance (stateful)
        self.character_cyberia_view = CharacterCyberiaView(
            object_layer_render_instance=self.object_layer_render
        )

        # Initialize Character View Modal
        self.modal_character_view = ModalCoreComponent(
            screen_width=SCREEN_WIDTH,
            screen_height=SCREEN_HEIGHT,
            render_content_callback=partial(render_modal_character_view_content),
            width=300,
            height=SCREEN_HEIGHT,
            padding_bottom=0,
            padding_right=0,
            horizontal_offset=0,
            background_color=Color(0, 0, 0, 200),
            title_text=self.character_cyberia_view.title_text,
        )
        self.show_modal_character_view = False

        # Initialize Chat View Modal
        self.modal_chat_view = ModalCoreComponent(
            screen_width=SCREEN_WIDTH,
            screen_height=SCREEN_HEIGHT,
            render_content_callback=partial(render_modal_chat_view_content),
            width=300,
            height=SCREEN_HEIGHT,
            padding_bottom=0,
            padding_right=0,
            horizontal_offset=0,
            background_color=Color(0, 0, 0, 200),
            title_text="Chat",
        )
        self.show_modal_chat_view = False

        # Initialize Map View Modal
        self.modal_map_view = ModalCoreComponent(
            screen_width=SCREEN_WIDTH,
            screen_height=SCREEN_HEIGHT,
            render_content_callback=partial(render_modal_map_view_content),
            width=300,
            height=SCREEN_HEIGHT,
            padding_bottom=0,
            padding_right=0,
            horizontal_offset=0,
            background_color=Color(0, 0, 0, 200),
            title_text="Map",
        )
        self.show_modal_map_view = False

        # Initialize the close button for modal_bag_view and modal_quest_list_view
        try:
            self.close_icon_texture = load_texture("ui/assets/icons/close.png")
        except Exception as e:
            logging.error(f"Failed to load close.png texture: {e}")
            self.close_icon_texture = None

        self.modal_right_panel_close_btn = (
            ModalCoreComponent(  # Renamed for reusability
                screen_width=SCREEN_WIDTH,
                screen_height=SCREEN_HEIGHT,
                render_content_callback=partial(render_modal_close_btn_content),
                width=30,
                height=30,
                padding_bottom=SCREEN_HEIGHT - 35,  # 5px from top, 30px height
                padding_right=5,  # 5px from right
                horizontal_offset=0,
                background_color=Color(
                    10, 10, 10, 100
                ),  # Dark background for consistency
                icon_texture=self.close_icon_texture,  # Pass the loaded texture
            )
        )
        self.show_modal_right_panel_close_btn = False  # State to control visibility

        # Initialize the arrow-left button for modal_bag_to_grid_btn
        try:
            self.arrow_left_icon_texture = load_texture(
                "ui/assets/icons/arrow-left.png"
            )
        except Exception as e:
            logging.error(f"Failed to load arrow-left.png texture: {e}")
            self.arrow_left_icon_texture = None

        # Initialize the "Back to Grid" button for modal_bag_view (adjacent to close button)
        self.modal_bag_to_grid_btn = ModalCoreComponent(
            screen_width=SCREEN_WIDTH,
            screen_height=SCREEN_HEIGHT,
            render_content_callback=partial(
                render_modal_close_btn_content
            ),  # Use close button content renderer for consistency
            width=30,  # A bit wider than close button
            height=30,
            padding_bottom=SCREEN_HEIGHT - 35,  # Same vertical position as close button
            padding_right=5
            + 30
            + 5,  # 5px from close button + close button width + its padding
            horizontal_offset=0,
            background_color=Color(10, 10, 10, 100),  # Dark background as requested
            icon_texture=self.arrow_left_icon_texture,  # Assign the arrow-left icon
        )
        self.show_modal_bag_to_grid_btn = False  # Initially hidden

        # Initialize the "Back to Quest List" button (similar to bag's back button)
        self.modal_quest_to_list_btn = ModalCoreComponent(
            screen_width=SCREEN_WIDTH,
            screen_height=SCREEN_HEIGHT,
            render_content_callback=partial(render_modal_close_btn_content),
            width=30,
            height=30,
            padding_bottom=SCREEN_HEIGHT - 35,
            padding_right=5 + 30 + 5,
            horizontal_offset=0,
            background_color=Color(10, 10, 10, 100),
            icon_texture=self.arrow_left_icon_texture,  # Reusing arrow-left icon
        )
        self.show_modal_quest_to_list_btn = False

        # New: Initialize the "Back to Character Grid" button
        self.modal_character_to_grid_btn = ModalCoreComponent(
            screen_width=SCREEN_WIDTH,
            screen_height=SCREEN_HEIGHT,
            render_content_callback=partial(render_modal_close_btn_content),
            width=30,
            height=30,
            padding_bottom=SCREEN_HEIGHT - 35,
            padding_right=5 + 30 + 5,  # Positioned next to close button
            horizontal_offset=0,
            background_color=Color(10, 10, 10, 100),
            icon_texture=self.arrow_left_icon_texture,
        )
        self.show_modal_character_to_grid_btn = False

        # Initialize the five small modals for UI MMO boxes (bottom-right)
        self.modal_btn_icon_modals = []
        icon_paths = [
            "ui/assets/icons/character.png",
            "ui/assets/icons/bag.png",
            "ui/assets/icons/chat.png",
            "ui/assets/icons/quest.png",
            "ui/assets/icons/map.png",
        ]
        self.modal_icons = []
        for path in icon_paths:
            try:
                texture = load_texture(path)
                self.modal_icons.append(texture)
            except Exception as e:
                logging.error(f"Failed to load texture {path}: {e}")
                self.modal_icons.append(None)

        num_modal_btn_icon_modals = 5

        for i in range(num_modal_btn_icon_modals):
            horizontal_offset = i * (40 + 5)

            icon_texture = self.modal_icons[i] if i < len(self.modal_icons) else None
            modal_title = ["Character", "Bag", "Chat", "Quest", "Map"][
                i
            ]  # Define titles for buttons
            modal = ModalCoreComponent(
                screen_width=SCREEN_WIDTH,
                screen_height=SCREEN_HEIGHT,
                render_content_callback=partial(render_modal_btn_icon_content),
                width=40,
                height=40,
                padding_bottom=5,
                padding_right=5,
                horizontal_offset=horizontal_offset,
                background_color=Color(*UI_MODAL_BACKGROUND_COLOR),
                icon_texture=icon_texture,  # Pass icon texture to modal
                title_text=modal_title,  # Pass title text to modal
            )
            self.modal_btn_icon_modals.append(modal)

        # Initialize the InteractionManager
        self.interaction_manager = InteractionManager(NETWORK_OBJECT_SIZE)

        # Track the currently active right-panel modal for toggle behavior
        self.active_right_panel_modal: str | None = (
            None  # "bag", "quest", "character", "chat", "map"
        )

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

    def _hide_all_right_panel_modals(self):
        """
        Helper to hide all right-panel modals and associated back/close buttons.
        This function does NOT reset the view states.
        """
        self.show_modal_bag_view = False
        self.show_modal_quest_list_view = False
        self.show_modal_character_view = False
        self.show_modal_chat_view = False
        self.show_modal_map_view = False
        self.show_modal_right_panel_close_btn = False
        self.show_modal_bag_to_grid_btn = False
        self.show_modal_quest_to_list_btn = False
        self.show_modal_character_to_grid_btn = False  # New: Hide character back button
        self.active_right_panel_modal = None

        # Reset modal titles to their default *only if they are views without internal state*
        # Views with internal state (Bag, Quest, Character) will handle their own title setting
        self.modal_chat_view.set_title("Chat")
        self.modal_map_view.set_title("Map")

    def run(self):
        """Runs the main client loop."""
        self.proxy.connect()

        if not self.connection_ready_event.wait(timeout=10):
            logging.error("Failed to establish connection via proxy within timeout.")
            for texture in self.modal_icons:
                if texture:
                    unload_texture(texture)
            if self.close_icon_texture:
                unload_texture(self.close_icon_texture)
            if self.arrow_left_icon_texture:
                unload_texture(self.arrow_left_icon_texture)
            self.object_layer_render.close_window()
            self.proxy.close()
            return

        last_frame_time = time.time()

        while not self.object_layer_render.window_should_close():
            current_time = time.time()
            delta_time = current_time - last_frame_time
            last_frame_time = current_time

            mouse_pos = get_mouse_position()
            mouse_x, mouse_y = int(mouse_pos.x), int(mouse_pos.y)
            is_mouse_left_button_pressed = (
                self.object_layer_render.is_mouse_button_pressed(MOUSE_BUTTON_LEFT)
            )

            # Handle window resize
            if is_window_resized():
                self.camera_manager.handle_window_resize()
                new_screen_width = get_screen_width()
                new_screen_height = get_screen_height()

                # Update quest discovery modal dimensions and position
                self.modal_quest_discovery.screen_width = new_screen_width
                self.modal_quest_discovery.screen_height = new_screen_height
                # Using explicit values for width (280), height (80), padding_top (5), and padding_right (5)
                self.modal_quest_discovery.x = new_screen_width - 280 - 5
                self.modal_quest_discovery.y = (
                    new_screen_height - (new_screen_height - 5 - 80) - 80
                )

                # Update right panel modals dimensions and position
                for modal in [
                    self.modal_bag_view,
                    self.modal_quest_list_view,
                    self.modal_character_view,
                    self.modal_chat_view,
                    self.modal_map_view,
                ]:
                    modal.screen_width = new_screen_width
                    modal.screen_height = new_screen_height
                    modal.x = new_screen_width - modal.width
                    modal.y = 0  # Top of the screen

                # Update modal_right_panel_close_btn dimensions and position
                self.modal_right_panel_close_btn.screen_width = new_screen_width
                self.modal_right_panel_close_btn.screen_height = new_screen_height
                self.modal_right_panel_close_btn.x = (
                    new_screen_width - self.modal_right_panel_close_btn.width - 5
                )
                self.modal_right_panel_close_btn.y = 5  # 5px padding from top

                # Update back buttons dimensions and position
                for modal in [
                    self.modal_bag_to_grid_btn,
                    self.modal_quest_to_list_btn,
                    self.modal_character_to_grid_btn,
                ]:  # New: include character back button
                    modal.screen_width = new_screen_width
                    modal.screen_height = new_screen_height
                    modal.x = self.modal_right_panel_close_btn.x - modal.width - 5
                    modal.y = 5

                # Update modal button icon modals dimensions and positions
                for i, modal in enumerate(self.modal_btn_icon_modals):
                    modal.screen_width = new_screen_width
                    modal.screen_height = new_screen_height
                    horizontal_offset = i * (40 + 5)
                    modal.x = (
                        modal.screen_width
                        - modal.width
                        - modal.padding_right
                        - horizontal_offset
                    )
                    modal.y = modal.screen_height - modal.height - modal.padding_bottom

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

            # Check for BOT-QUEST-PROVIDER interaction and control quest discovery modal visibility
            # modal_quest_discovery should not be activated if any right panel is active
            right_panel_active = self.active_right_panel_modal is not None
            if not right_panel_active:
                self.show_modal_quest_discovery = (
                    self.interaction_manager.check_for_bot_interaction(
                        self.network_state, self.my_player_id
                    )
                )
            else:
                self.show_modal_quest_discovery = False

            # Check for modal clicks BEFORE processing world clicks
            modal_was_clicked_this_frame = False
            current_mouse_is_pressed = (
                is_mouse_left_button_pressed  # Store original state for checks
            )

            # --- Handle clicks for the right panel modals first, if they are active ---
            if self.show_modal_right_panel_close_btn:
                if self.modal_right_panel_close_btn.check_click(
                    mouse_x, mouse_y, current_mouse_is_pressed
                ):
                    logging.info(
                        "Close button clicked. Closing right panel views and resetting state."
                    )
                    # Explicitly reset the view of the currently active modal
                    if self.active_right_panel_modal == "bag":
                        self.bag_cyberia_view.reset_view()
                    elif self.active_right_panel_modal == "quest":
                        self.quest_cyberia_view.reset_view()
                    elif self.active_right_panel_modal == "character":
                        self.character_cyberia_view.reset_view()
                    # Then hide all modals
                    self._hide_all_right_panel_modals()
                    modal_was_clicked_this_frame = True  # Consume the click

            if self.show_modal_bag_view:
                if current_mouse_is_pressed and (
                    mouse_x >= self.modal_bag_view.x
                    and mouse_x <= (self.modal_bag_view.x + self.modal_bag_view.width)
                    and mouse_y >= self.modal_bag_view.y
                    and mouse_y <= (self.modal_bag_view.y + self.modal_bag_view.height)
                ):
                    modal_was_clicked_this_frame = True

                player_obj_layer_ids = []
                with self.network_state.lock:
                    if self.my_network_object:
                        player_obj_layer_ids = self.my_network_object.object_layer_ids

                if (
                    self.show_modal_bag_to_grid_btn
                    and self.modal_bag_to_grid_btn.check_click(
                        mouse_x, mouse_y, current_mouse_is_pressed
                    )
                ):
                    logging.info("Back to Grid button clicked. Resetting bag view.")
                    self.bag_cyberia_view.reset_view()
                    self.show_modal_bag_to_grid_btn = False

                elif current_mouse_is_pressed:
                    if self.bag_cyberia_view.handle_slot_clicks(
                        self.modal_bag_view.x,
                        self.modal_bag_view.y,
                        self.modal_bag_view.width,
                        self.modal_bag_view.height,
                        player_obj_layer_ids,
                        mouse_x,
                        mouse_y,
                        current_mouse_is_pressed,
                    ):
                        self.show_modal_bag_to_grid_btn = (
                            self.bag_cyberia_view.selected_object_layer_id is not None
                        )

            elif self.show_modal_quest_list_view:
                if current_mouse_is_pressed and (
                    mouse_x >= self.modal_quest_list_view.x
                    and mouse_x
                    <= (self.modal_quest_list_view.x + self.modal_quest_list_view.width)
                    and mouse_y >= self.modal_quest_list_view.y
                    and mouse_y
                    <= (
                        self.modal_quest_list_view.y + self.modal_quest_list_view.height
                    )
                ):
                    modal_was_clicked_this_frame = True

                if (
                    self.show_modal_quest_to_list_btn
                    and self.modal_quest_to_list_btn.check_click(
                        mouse_x, mouse_y, current_mouse_is_pressed
                    )
                ):
                    logging.info(
                        "Back to Quest List button clicked. Resetting quest view."
                    )
                    self.quest_cyberia_view.reset_view()
                    self.show_modal_quest_to_list_btn = False

                elif current_mouse_is_pressed:
                    if self.quest_cyberia_view.handle_item_clicks(
                        self.modal_quest_list_view.x,
                        self.modal_quest_list_view.y,
                        self.modal_quest_list_view.width,
                        self.modal_quest_list_view.height,
                        mouse_x,
                        mouse_y,
                        current_mouse_is_pressed,
                    ):
                        self.show_modal_quest_to_list_btn = (
                            self.quest_cyberia_view.selected_quest_index is not None
                        )
            elif self.show_modal_character_view:
                if current_mouse_is_pressed and (
                    mouse_x >= self.modal_character_view.x
                    and mouse_x
                    <= (self.modal_character_view.x + self.modal_character_view.width)
                    and mouse_y >= self.modal_character_view.y
                    and mouse_y
                    <= (self.modal_character_view.y + self.modal_character_view.height)
                ):
                    modal_was_clicked_this_frame = True

                # New: Handle character back button click
                if (
                    self.show_modal_character_to_grid_btn
                    and self.modal_character_to_grid_btn.check_click(
                        mouse_x, mouse_y, current_mouse_is_pressed
                    )
                ):
                    logging.info(
                        "Back to Character Grid button clicked. Resetting character view."
                    )
                    self.character_cyberia_view.reset_view()
                    self.show_modal_character_to_grid_btn = False

                elif current_mouse_is_pressed:
                    if self.character_cyberia_view.handle_slot_clicks(
                        self.modal_character_view.x,
                        self.modal_character_view.y,
                        self.modal_character_view.width,
                        self.modal_character_view.height,
                        mouse_x,
                        mouse_y,
                        current_mouse_is_pressed,
                    ):
                        self.show_modal_character_to_grid_btn = (  # New: Show character back button when a detail is selected
                            self.character_cyberia_view.selected_slot_key is not None
                        )

            elif self.show_modal_chat_view:
                if current_mouse_is_pressed and self.modal_chat_view.check_click(
                    mouse_x, mouse_y, current_mouse_is_pressed
                ):
                    modal_was_clicked_this_frame = True
            elif self.show_modal_map_view:
                if current_mouse_is_pressed and self.modal_map_view.check_click(
                    mouse_x, mouse_y, current_mouse_is_pressed
                ):
                    modal_was_clicked_this_frame = True

            # Check quest discovery modal
            if self.show_modal_quest_discovery:
                if self.modal_quest_discovery.check_click(
                    mouse_x, mouse_y, current_mouse_is_pressed
                ):
                    modal_was_clicked_this_frame = True

            # Check general UI buttons (Character, Bag, Chat, Quest, Map)
            for i, modal_btn in enumerate(self.modal_btn_icon_modals):
                if modal_btn.check_click(mouse_x, mouse_y, current_mouse_is_pressed):
                    modal_was_clicked_this_frame = True
                    target_modal_key = ["character", "bag", "chat", "quest", "map"][i]

                    if self.active_right_panel_modal == target_modal_key:
                        # If clicked the active modal, close it WITHOUT resetting its view state
                        self._hide_all_right_panel_modals()
                        logging.info(
                            f"Closed {target_modal_key} modal without resetting view."
                        )
                    else:
                        # Close any currently open modal, AND reset its view state (it's the 'old' modal)
                        if self.active_right_panel_modal == "bag":
                            self.bag_cyberia_view.reset_view()
                            logging.info("Resetting Bag view state.")
                        elif self.active_right_panel_modal == "quest":
                            self.quest_cyberia_view.reset_view()
                            logging.info("Resetting Quest view state.")
                        elif self.active_right_panel_modal == "character":
                            self.character_cyberia_view.reset_view()
                            logging.info("Resetting Character view state.")
                        # Hide all modals (including the one just reset, if any)
                        self._hide_all_right_panel_modals()

                        # Open the newly clicked modal
                        if target_modal_key == "character":
                            self.show_modal_character_view = True
                        elif target_modal_key == "bag":
                            self.show_modal_bag_view = True
                        elif target_modal_key == "chat":
                            self.show_modal_chat_view = True
                        elif target_modal_key == "quest":
                            self.show_modal_quest_list_view = True
                        elif target_modal_key == "map":
                            self.show_modal_map_view = True

                        self.show_modal_right_panel_close_btn = True
                        self.active_right_panel_modal = target_modal_key
                        logging.info(f"Opened {target_modal_key} modal.")

                    # Update back button visibility based on the newly opened modal
                    self.show_modal_bag_to_grid_btn = (
                        self.show_modal_bag_view
                        and self.bag_cyberia_view.selected_object_layer_id is not None
                    )
                    self.show_modal_quest_to_list_btn = (
                        self.show_modal_quest_list_view
                        and self.quest_cyberia_view.selected_quest_index is not None
                    )
                    self.show_modal_character_to_grid_btn = (  # New: Set character back button visibility
                        self.show_modal_character_view
                        and self.character_cyberia_view.selected_slot_key is not None
                    )

            # Only process world clicks if no modal was clicked in this frame
            if not modal_was_clicked_this_frame and current_mouse_is_pressed:
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
            # Render right panel modals if active
            if self.show_modal_bag_view:
                player_obj_layer_ids = []
                with self.network_state.lock:
                    if self.my_network_object:
                        player_obj_layer_ids = self.my_network_object.object_layer_ids

                self.modal_bag_view.data_to_pass = {
                    "bag_view_instance": self.bag_cyberia_view,
                    "player_object_layer_ids": player_obj_layer_ids,
                    "mouse_x": mouse_x,
                    "mouse_y": mouse_y,
                    "is_mouse_button_pressed": is_mouse_left_button_pressed,
                }
                self.modal_bag_view.render(self.object_layer_render, mouse_x, mouse_y)
                self.show_modal_bag_to_grid_btn = (
                    self.bag_cyberia_view.selected_object_layer_id is not None
                )
            elif self.show_modal_quest_list_view:
                self.modal_quest_list_view.data_to_pass = {
                    "quest_view_instance": self.quest_cyberia_view,
                    "mouse_x": mouse_x,
                    "mouse_y": mouse_y,
                    "is_mouse_button_pressed": is_mouse_left_button_pressed,
                }
                self.modal_quest_list_view.render(
                    self.object_layer_render, mouse_x, mouse_y
                )
                self.show_modal_quest_to_list_btn = (
                    self.quest_cyberia_view.selected_quest_index is not None
                )
            elif self.show_modal_character_view:
                self.modal_character_view.data_to_pass = {
                    "character_view_instance": self.character_cyberia_view,
                    "mouse_x": mouse_x,
                    "mouse_y": mouse_y,
                    "is_mouse_button_pressed": is_mouse_left_button_pressed,
                }
                self.modal_character_view.render(
                    self.object_layer_render, mouse_x, mouse_y
                )

            elif self.show_modal_chat_view:
                self.modal_chat_view.render(self.object_layer_render, mouse_x, mouse_y)
            elif self.show_modal_map_view:
                self.modal_map_view.render(self.object_layer_render, mouse_x, mouse_y)

            # Render close button and back-to-grid button if a right panel is open
            if self.show_modal_right_panel_close_btn:
                self.modal_right_panel_close_btn.render(
                    self.object_layer_render, mouse_x, mouse_y
                )

            # The "Back to Grid" button is only rendered when an item is selected AND the bag view is open
            if self.show_modal_bag_to_grid_btn and self.show_modal_bag_view:
                self.modal_bag_to_grid_btn.render(
                    self.object_layer_render, mouse_x, mouse_y
                )

            # The "Back to Quest List" button is only rendered when a quest is selected AND the quest view is open
            if self.show_modal_quest_to_list_btn and self.show_modal_quest_list_view:
                self.modal_quest_to_list_btn.render(
                    self.object_layer_render, mouse_x, mouse_y
                )

            # New: The "Back to Character Grid" button is only rendered when an item is selected AND the character view is open
            if self.show_modal_character_to_grid_btn and self.show_modal_character_view:
                self.modal_character_to_grid_btn.render(
                    self.object_layer_render, mouse_x, mouse_y
                )

            if self.show_modal_quest_discovery:
                # Pass mouse_x, mouse_y to enable hover effect logic in ModalCoreComponent
                self.modal_quest_discovery.render(
                    self.object_layer_render, mouse_x, mouse_y
                )

            # Render modal button icon modals, passing mouse coordinates for hover effect
            for modal in self.modal_btn_icon_modals:
                modal.render(self.object_layer_render, mouse_x, mouse_y)

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

        for texture in self.modal_icons:
            if texture:
                unload_texture(texture)
        if self.close_icon_texture:
            unload_texture(self.close_icon_texture)
        if self.arrow_left_icon_texture:  # Unload new texture
            unload_texture(self.arrow_left_icon_texture)

        self.proxy.close()
        self.object_layer_render.close_window()
        logging.info("Client window closed.")
