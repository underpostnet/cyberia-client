import logging
from typing import Callable, Dict, Any, List
from functools import partial

from raylibpy import (
    Color,
    is_mouse_button_down,
    MOUSE_BUTTON_LEFT,
)  # Import is_mouse_button_down

from ui.components.core.modal_core_component import ModalCoreComponent
from ui.components.core.texture_manager import TextureManager
from ui.components.core.keyboard_core_component import KeyboardCoreComponent
from object_layer.object_layer_render import ObjectLayerRender
from ui.components.core.modal_main_bar_component import (
    ModalMainBarComponent,
)  # Updated Import Path
from network_state.network_state_proxy import NetworkStateProxy # For passing to MapView


logging.basicConfig(
    level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s"
)


class RouterCoreComponent:
    """
    Manages the navigation between different UI views (represented by modals),
    abstracting the switching logic. It also handles the creation and rendering
    of navigation buttons based on defined routes.

    A route is defined by a dictionary with 'name', 'path', 'icon_path',
    'view_instance', and 'render_callback'.
    """

    def __init__(
        self,
        screen_width: int,
        screen_height: int,
        object_layer_render_instance: ObjectLayerRender,
        texture_manager: TextureManager,
        keyboard_core_component: KeyboardCoreComponent,
        network_proxy: NetworkStateProxy, # Added network_proxy
        routes: List[Dict[str, Any]],
        ui_modal_background_color: Color,
        modal_width: int = 300,
        modal_height: int = 0,  # Will be SCREEN_HEIGHT
        modal_padding_right: int = 0,
        modal_padding_bottom: int = 0,
        btn_modal_width: int = 40,
        btn_modal_height: int = 40,
        btn_modal_padding_bottom: int = 5,
        btn_modal_padding_right: int = 5,
        close_btn_width: int = 30,
        close_btn_height: int = 30,
        close_btn_padding_bottom: int = 0,  # Top of screen
        close_btn_padding_right: int = 5,
        back_btn_offset_from_close: int = 5,  # Distance from close button
        # New offsets for maximize button positioning
        maximize_btn_normal_x_offset_from_modal_left: int = 5,  # Offset from left edge of modal in normal mode
        maximize_btn_normal_top_padding_from_modal: int = 5,
        maximize_btn_maximized_x_offset_from_screen_left: int = 5,  # Offset from left edge of screen in maximized mode
        maximize_btn_maximized_top_padding_from_screen: int = 5,
        close_btn_texture_path: str = "ui/assets/icons/close.png",
        back_btn_texture_path: str = "ui/assets/icons/arrow-left.png",
        maximize_btn_texture_path: str = "ui/assets/icons/maximize.png",  # New texture path
        render_modal_btn_icon_content_callback: Callable = None,
        render_modal_close_btn_content_callback: Callable = None,
    ):
        self.screen_width = screen_width
        self.screen_height = screen_height
        self.object_layer_render = object_layer_render_instance
        self.texture_manager = texture_manager
        self.keyboard_core_component = keyboard_core_component
        self.network_proxy = network_proxy # Store the proxy
        self.ui_modal_background_color = ui_modal_background_color

        self.routes = routes
        self.active_route_path: str | None = None
        self.active_view_modal: ModalCoreComponent | None = None

        # Configuration for modals
        self.modal_width = modal_width
        self.modal_height = modal_height if modal_height else screen_height
        self.modal_padding_right = modal_padding_right
        self.modal_padding_bottom = modal_padding_bottom

        # Configuration for button modals (bottom right UI elements) - now managed by ModalMainBarComponent
        self.btn_modal_width = btn_modal_width
        self.btn_modal_height = btn_modal_height
        self.btn_modal_padding_bottom = btn_modal_padding_bottom
        self.btn_modal_padding_right = btn_modal_padding_right

        # Configuration for close button
        self.close_btn_width = close_btn_width
        self.close_btn_height = close_btn_height
        # Calculate close button position from top-right
        self.close_btn_x_offset = close_btn_padding_right
        self.close_btn_y_offset = close_btn_padding_bottom

        # Configuration for back button
        self.back_btn_offset_from_close = back_btn_offset_from_close

        # Configuration for maximize button offsets
        self.maximize_btn_normal_x_offset_from_modal_left = (
            maximize_btn_normal_x_offset_from_modal_left
        )
        self.maximize_btn_normal_top_padding_from_modal = (
            maximize_btn_normal_top_padding_from_modal
        )
        self.maximize_btn_maximized_x_offset_from_screen_left = (
            maximize_btn_maximized_x_offset_from_screen_left
        )
        self.maximize_btn_maximized_top_padding_from_screen = (
            maximize_btn_maximized_top_padding_from_screen
        )

        self.render_modal_btn_icon_content_callback = (
            render_modal_btn_icon_content_callback
        )
        self.render_modal_close_btn_content_callback = (
            render_modal_close_btn_content_callback
        )

        # Load global textures
        self.close_icon_texture = self.texture_manager.load_texture(
            close_btn_texture_path
        )
        self.arrow_left_icon_texture = self.texture_manager.load_texture(
            back_btn_texture_path
        )
        self.maximize_icon_texture = (
            self.texture_manager.load_texture(  # Load maximize texture
                maximize_btn_texture_path
            )
        )

        # Initialize the main bar component
        self.main_bar = ModalMainBarComponent(
            screen_width=self.screen_width,
            screen_height=self.screen_height,
            object_layer_render_instance=self.object_layer_render,
            texture_manager=self.texture_manager,
            routes=self.routes,  # Pass all routes, main bar will filter for navigation buttons
            ui_modal_background_color=self.ui_modal_background_color,
            btn_modal_width=self.btn_modal_width,
            btn_modal_height=self.btn_modal_height,
            btn_modal_padding_bottom=self.btn_modal_padding_bottom,
            btn_modal_padding_right=self.btn_modal_padding_right,
            render_modal_btn_icon_content_callback=self.render_modal_btn_icon_content_callback,
        )

        # Adjust modal height based on main bar height
        # The main modals should not overlap with the main bar
        self.modal_height_adjusted = self.screen_height - self.main_bar.height

        self._initialize_view_modals()
        # Navigation buttons are now managed by main_bar, so remove _initialize_navigation_buttons() call
        self._initialize_control_buttons()

    def _initialize_view_modals(self):
        """
        Initializes the ModalCoreComponent instances for each view.
        These are the large modals that show the actual UI content (Bag, Quest, etc.).
        The height is adjusted to account for the main bar at the bottom.
        """
        self.view_modals: Dict[str, ModalCoreComponent] = {}
        for route in self.routes:
            modal = ModalCoreComponent(
                screen_width=self.screen_width,
                screen_height=self.screen_height,
                render_content_callback=partial(route["render_callback"]),
                width=self.modal_width,
                height=self.modal_height_adjusted,  # Use adjusted height
                padding_bottom=self.main_bar.height,  # Start modals above the main bar
                padding_right=self.modal_padding_right,
                horizontal_offset=0,
                background_color=Color(0, 0, 0, 200),  # Slightly darker for view
                title_text=route["view_instance"].title_text,  # Initial title from view
            )
            self.view_modals[route["path"]] = modal
            # Assign the modal instance back to the route for easy access during rendering
            route["modal_instance"] = modal

    # Remove _initialize_navigation_buttons as it's now in ModalMainBarComponent
    # def _initialize_navigation_buttons(self):
    #     ...

    def _initialize_control_buttons(self):
        """
        Initializes the close and back buttons for the right-panel modals.
        """
        # Close button for right panel modals
        self.close_button = ModalCoreComponent(
            screen_width=self.screen_width,
            screen_height=self.screen_height,
            render_content_callback=partial(
                self.render_modal_close_btn_content_callback
            ),
            width=self.close_btn_width,
            height=self.close_btn_height,
            padding_bottom=self.screen_height
            - self.close_btn_height
            - self.close_btn_y_offset,  # Position from top
            padding_right=self.close_btn_x_offset,  # Position from right
            horizontal_offset=0,
            background_color=Color(10, 10, 10, 100),
            icon_texture=self.close_icon_texture,
            title_text="Close",
        )

        # Back button for detail views within right panel modals
        self.back_button = ModalCoreComponent(
            screen_width=self.screen_width,
            screen_height=self.screen_height,
            render_content_callback=partial(
                self.render_modal_close_btn_content_callback
            ),
            width=self.close_btn_width,  # Same size as close button
            height=self.close_btn_height,
            padding_bottom=self.screen_height
            - self.close_btn_height
            - self.close_btn_y_offset,  # Position from top
            padding_right=self.close_btn_x_offset
            + self.close_btn_width
            + self.back_btn_offset_from_close,
            horizontal_offset=0,
            background_color=Color(10, 10, 10, 100),
            icon_texture=self.arrow_left_icon_texture,
            title_text="Back",
        )
        self.show_back_button = False

        # Maximize button. Its position will be dynamically set in the render loop.
        # Initialize with minimal padding to ensure ModalCoreComponent's internal x,y donop interfere.
        self.maximize_button = ModalCoreComponent(
            screen_width=self.screen_width,
            screen_height=self.screen_height,
            render_content_callback=partial(
                self.render_modal_close_btn_content_callback  # Using same render callback for icon
            ),
            width=self.close_btn_width,  # Same size as close button
            height=self.close_btn_height,
            padding_bottom=0,  # Will be set dynamically
            padding_right=0,  # Will be set dynamically
            horizontal_offset=0,
            background_color=Color(10, 10, 10, 100),
            icon_texture=self.maximize_icon_texture,
            title_text="Maximize",
        )
        self.show_maximize_button = False  # Controls visibility of maximize button

    def navigate_to(self, path: str):
        """
        Navigates to the specified route path, activating its associated view modal.
        Resets the previous active view's state before switching.
        If the path is already active, it deactivates it (toggles off).
        """
        # Close the currently active modal, and reset its view's state
        if self.active_route_path:
            for route in self.routes:
                if route["path"] == self.active_route_path:
                    # Reset the view instance associated with the closing modal
                    if hasattr(route["view_instance"], "reset_view"):
                        route["view_instance"].reset_view()
                        logging.info(f"Reset view state for {self.active_route_path}.")
                    break

        if self.active_route_path == path:
            # If clicking the currently active route, toggle it off
            self.active_route_path = None
            self.active_view_modal = None
            self.show_back_button = (
                False  # Hide back button when no main modal is active
            )
            self.show_maximize_button = (
                False  # Hide maximize button when no main modal is active
            )
            logging.info(f"Deactivated route: {path}")
        else:
            # Activate the new route
            self.active_route_path = path
            self.active_view_modal = self.view_modals.get(path)
            self.show_maximize_button = (
                True  # Show maximize button when a modal is active
            )
            logging.info(f"Activated route: {path}")

        # Ensure back button state is updated after navigation
        self._update_back_button_visibility()

    def _update_back_button_visibility(self):
        """Updates the visibility of the back button based on the active view's internal state."""
        self.show_back_button = False
        if self.active_route_path:
            for route in self.routes:
                if route["path"] == self.active_route_path:
                    view_instance = route["view_instance"]
                    if (
                        hasattr(view_instance, "selected_quest_index")
                        and view_instance.selected_quest_index is not None
                    ):
                        self.show_back_button = True
                    elif (
                        hasattr(view_instance, "selected_object_layer_id")
                        and view_instance.selected_object_layer_id is not None
                    ):
                        self.show_back_button = True
                    elif (
                        hasattr(view_instance, "selected_slot_key")
                        and view_instance.selected_slot_key is not None
                    ):
                        self.show_back_button = True
                    elif (
                        hasattr(view_instance, "selected_chat_index")
                        and view_instance.selected_chat_index is not None
                    ):
                        self.show_back_button = True
                    break  # Found the active route, no need to continue

    def handle_navigation_button_clicks(
        self, mouse_x: int, mouse_y: int, is_mouse_button_pressed: bool
    ) -> bool:
        """
        Handles clicks on the bottom-right navigation buttons.
        Delegates to ModalMainBarComponent.
        Returns True if a button was clicked.
        """
        route_path = self.main_bar.handle_clicks(
            mouse_x, mouse_y, is_mouse_button_pressed
        )
        if route_path:
            self.navigate_to(route_path)
            return True
        return False

    def handle_control_button_clicks(
        self, mouse_x: int, mouse_y: int, is_mouse_button_pressed: bool
    ) -> bool:
        """
        Handles clicks on the close, back, and maximize buttons.
        Returns True if a button was clicked.
        """
        if self.active_view_modal and self.close_button.check_click(
            mouse_x, mouse_y, is_mouse_button_pressed
        ):
            logging.info("Close button clicked via Router. Deactivating active view.")
            # If closing from a maximized state, reset modal to original size first
            if self.active_view_modal.is_maximized:
                self._toggle_maximize_active_modal()
            self.navigate_to(self.active_route_path)  # Toggles off the current view
            return True

        if self.show_back_button and self.back_button.check_click(
            mouse_x, mouse_y, is_mouse_button_pressed
        ):
            logging.info(
                "Back button clicked via Router. Resetting active view's internal state."
            )
            # Find the active view instance and call its reset_view method
            for route in self.routes:
                if route["path"] == self.active_route_path:
                    if hasattr(route["view_instance"], "reset_view"):
                        route["view_instance"].reset_view()
                        logging.info(
                            f"Reset internal state of {self.active_route_path}."
                        )
                    break
            self._update_back_button_visibility()  # Update back button visibility after reset
            return True

        # Handle maximize button click
        if self.show_maximize_button and self.maximize_button.check_click(
            mouse_x, mouse_y, is_mouse_button_pressed
        ):
            self._toggle_maximize_active_modal()
            return True

        return False

    def _toggle_maximize_active_modal(self):
        """Toggles the maximized state of the active view modal."""
        if self.active_view_modal:
            if self.active_view_modal.is_maximized:
                # Restore original size
                self.active_view_modal.set_maximized_state(
                    False,
                    self.active_view_modal.original_width,
                    self.active_view_modal.original_padding_right,
                    self.active_view_modal.original_height,
                    self.active_view_modal.original_padding_bottom,
                )
                logging.info(f"Restored modal size for {self.active_route_path}")
            else:
                # Maximize to full screen (entire width), height adjusted for main bar
                available_width = self.screen_width
                # New height for the modal when maximized, considering the main bar at the bottom
                maximized_height = self.screen_height - self.main_bar.height

                # We need to set the padding_bottom for the maximized modal so it sits just above the main bar
                # The padding_bottom of the modal will be the height of the main bar
                new_padding_bottom = self.main_bar.height

                self.active_view_modal.set_maximized_state(
                    True,
                    available_width,
                    0,
                    maximized_height,
                    new_padding_bottom,  # Pass new height and padding_bottom
                )
                logging.info(f"Maximized modal size for {self.active_route_path}")

    def render(
        self, mouse_x: int, mouse_y: int, is_mouse_button_pressed: bool, dt: float = 0.0
    ):
        """
        Renders the active view modal and all control buttons and the main bar.
        """
        # Determine if the mouse button is currently held down
        is_mouse_left_button_down = is_mouse_button_down(MOUSE_BUTTON_LEFT)

        # Render the currently active view modal
        if self.active_view_modal:
            # Pass mouse coordinates and button state to the view's render_content
            current_route_data = next(
                (r for r in self.routes if r["path"] == self.active_route_path), None
            )
            if current_route_data:
                view_instance = current_route_data["view_instance"]
                self.active_view_modal.data_to_pass.update(
                    {  # Use update to preserve existing data
                        f"{current_route_data['name'].lower().replace(' ', '_')}_view_instance": view_instance,
                        "mouse_x": mouse_x,
                        "mouse_y": mouse_y,
                        "is_mouse_button_pressed": is_mouse_button_pressed,
                        "is_mouse_button_down": is_mouse_left_button_down,  # Pass mouse button down state
                        "char_pressed": self.keyboard_core_component.get_char_pressed(),
                        "key_pressed": self.keyboard_core_component.get_key_pressed(),
                        "is_key_down_map": self.keyboard_core_component.get_is_key_down_map(),
                        "dt": dt,
                        "current_channel_id": self.network_proxy.current_channel_id, # Pass current channel
                    }
                )
                # Update modal title dynamically from the view instance
                self.active_view_modal.set_title(view_instance.title_text)
                self.active_view_modal.render(
                    self.object_layer_render,
                    mouse_x,
                    mouse_y,
                    is_mouse_left_button_down,  # Pass is_mouse_button_down
                )
                # After rendering, update the back button visibility based on the view's potentially changed state
                self._update_back_button_visibility()

        # Render close and back buttons if a view modal is active
        if self.active_view_modal:
            self.close_button.render(
                self.object_layer_render, mouse_x, mouse_y, is_mouse_left_button_down
            )
            if self.show_back_button:
                self.back_button.render(
                    self.object_layer_render,
                    mouse_x,
                    mouse_y,
                    is_mouse_left_button_down,
                )

            # Position and render maximize button based on modal state
            if self.show_maximize_button:
                if self.active_view_modal.is_maximized:
                    # Position relative to screen's top-left when maximized
                    self.maximize_button.x = (
                        self.maximize_btn_maximized_x_offset_from_screen_left
                    )
                    self.maximize_button.y = (
                        self.maximize_btn_maximized_top_padding_from_screen
                    )
                else:
                    # Position relative to modal's top-left in normal mode
                    self.maximize_button.x = (
                        self.active_view_modal.x
                        + self.maximize_btn_normal_x_offset_from_modal_left
                    )
                    self.maximize_button.y = (
                        self.active_view_modal.y
                        + self.maximize_btn_normal_top_padding_from_modal
                    )
                self.maximize_button.render(
                    self.object_layer_render,
                    mouse_x,
                    mouse_y,
                    is_mouse_left_button_down,
                )

        # Render the main bar with navigation buttons
        self.main_bar.render(mouse_x, mouse_y)

    def handle_view_clicks(
        self, mouse_x: int, mouse_y: int, is_mouse_button_pressed: bool
    ) -> bool:
        """
        Allows the active view modal to handle its internal clicks.
        This method is called *after* global router buttons are checked.
        Returns True if the active view handled a click.
        """
        if self.active_view_modal:
            active_route = next(
                (r for r in self.routes if r["path"] == self.active_route_path), None
            )
            if active_route:
                view_instance = active_route["view_instance"]
                handled = False
                # Determine which click handler to call based on the view type
                if hasattr(
                    view_instance, "handle_item_clicks"
                ):  # For Bag, Quest, Chat, Map
                    handled = view_instance.handle_item_clicks(
                        self.active_view_modal.x,
                        self.active_view_modal.y,
                        self.active_view_modal.width,
                        self.active_view_modal.height,
                        mouse_x,
                        mouse_y,
                        is_mouse_button_pressed,
                    )
                elif hasattr(view_instance, "handle_slot_clicks"):  # For Character
                    handled = view_instance.handle_slot_clicks(
                        self.active_view_modal.x,
                        self.active_view_modal.y,
                        self.active_view_modal.width,
                        self.active_view_modal.height,
                        mouse_x,
                        mouse_y,
                        is_mouse_button_pressed,
                    )

                if handled:  # If any click was handled, update back button visibility
                    self._update_back_button_visibility()
                return handled
        return False

    def update_screen_dimensions(self, new_screen_width: int, new_screen_height: int):
        """
        Updates the screen dimensions for all modals and repositions them.
        """
        self.screen_width = new_screen_width
        self.screen_height = new_screen_height

        # Update the main bar component's screen dimensions and reposition its buttons
        self.main_bar.update_screen_dimensions(new_screen_width, new_screen_height)

        # Recalculate adjusted modal height based on new screen height and main bar height
        self.modal_height_adjusted = new_screen_height - self.main_bar.height

        # Update view modals
        for modal in self.view_modals.values():
            modal.screen_width = new_screen_width
            modal.screen_height = (
                new_screen_height  # Pass full screen height for internal calculations
            )

            # Update modal height and padding_bottom to ensure it fits above the main bar
            modal.height = self.modal_height_adjusted
            modal.padding_bottom = self.main_bar.height

            # If modal is maximized, recalculate its maximized position
            if modal.is_maximized:
                # When maximized, modal takes full screen width, and adjusted height
                modal.set_maximized_state(
                    True, new_screen_width, 0, modal.height, modal.padding_bottom
                )
            else:
                # In normal mode, reposition based on its original width and the new adjusted height/padding
                modal.x = new_screen_width - modal.width - modal.padding_right
                modal.y = new_screen_height - modal.height - modal.padding_bottom

        # Update close button
        self.close_button.screen_width = new_screen_width
        self.close_button.screen_height = new_screen_height
        self.close_button.x = (
            new_screen_width - self.close_button.width - self.close_btn_x_offset
        )
        self.close_button.y = self.close_btn_y_offset  # Stays at top of screen

        # Update back button
        self.back_button.screen_width = new_screen_width
        self.back_button.screen_height = new_screen_height
        self.back_button.x = (
            self.close_button.x
            - self.back_button.width
            - self.back_btn_offset_from_close
        )
        self.back_button.y = self.close_btn_y_offset  # Stays at top of screen

        # Update maximize button. Its position is handled dynamically in render.
        self.maximize_button.screen_width = new_screen_width
        self.maximize_button.screen_height = new_screen_height

        # Navigation buttons update is now handled by self.main_bar.update_screen_dimensions()
