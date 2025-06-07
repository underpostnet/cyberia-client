import logging
from typing import Callable, Dict, Any, List
from functools import partial

from raylibpy import (
    Color,
    Rectangle,
    Vector2,
)  # Added Vector2 for potential use in draw_rectangle
from ui.components.core.modal_core_component import ModalCoreComponent
from ui.components.core.texture_manager import TextureManager
from object_layer.object_layer_render import ObjectLayerRender


logging.basicConfig(
    level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s"
)


class ModalMainBarComponent:
    """
    Manages and renders the main navigation bar at the bottom of the screen.
    This component includes buttons for Character, Bag, Chat, Quest, and Map views.
    """

    def __init__(
        self,
        screen_width: int,
        screen_height: int,
        object_layer_render_instance: ObjectLayerRender,
        texture_manager: TextureManager,
        routes: List[Dict[str, Any]],
        ui_modal_background_color: Color,  # This is the default background color from config
        btn_modal_width: int = 40,
        btn_modal_height: int = 40,
        btn_modal_padding_bottom: int = 5,
        btn_modal_padding_right: int = 5,
        render_modal_btn_icon_content_callback: Callable = None,
    ):
        self.screen_width = screen_width
        self.screen_height = screen_height
        self.object_layer_render = object_layer_render_instance
        self.texture_manager = texture_manager
        self.routes = routes
        self.ui_modal_background_color = ui_modal_background_color
        self.btn_modal_width = btn_modal_width
        self.btn_modal_height = btn_modal_height
        self.btn_modal_padding_bottom = btn_modal_padding_bottom
        self.btn_modal_padding_right = btn_modal_padding_right
        self.render_modal_btn_icon_content_callback = (
            render_modal_btn_icon_content_callback
        )

        self.navigation_buttons: List[ModalCoreComponent] = []
        self._initialize_navigation_buttons()

        # Calculate the height of the main bar. This defines the overall bar's height.
        self.height = (
            self.btn_modal_height + self.btn_modal_padding_bottom * 2
        )  # Add padding to top and bottom for the bar itself
        # Calculate the Y position of the main bar at the bottom of the screen
        self.y_position = self.screen_height - self.height

    def _initialize_navigation_buttons(self):
        """
        Initializes the ModalCoreComponent instances for navigation buttons
        (Character, Bag, Chat, Quest, Map) at the bottom right.
        """
        self.navigation_buttons: List[ModalCoreComponent] = []
        for i, route in enumerate(self.routes):
            icon_texture = self.texture_manager.load_texture(route["icon_path"])
            horizontal_offset = i * (
                self.btn_modal_width + self.btn_modal_padding_right
            )

            modal_btn = ModalCoreComponent(
                screen_width=self.screen_width,
                screen_height=self.screen_height,
                render_content_callback=partial(
                    self.render_modal_btn_icon_content_callback
                ),
                width=self.btn_modal_width,
                height=self.btn_modal_height,
                # Position buttons relative to the bar's bottom padding.
                # The bar's Y position is self.y_position.
                # Buttons should be placed at self.y_position + self.btn_modal_padding_bottom
                padding_bottom=self.btn_modal_padding_bottom,  # This padding is *from the bottom of the button*
                padding_right=self.btn_modal_padding_right,
                horizontal_offset=horizontal_offset,
                background_color=self.ui_modal_background_color,  # Use the default background color
                icon_texture=icon_texture,
                title_text=route["name"],  # Name for the button
            )
            self.navigation_buttons.append(modal_btn)
            # Store a reference to the route in the button's data for click handling
            modal_btn.data_to_pass["route_path"] = route["path"]

    def render(self, mouse_x: int, mouse_y: int):
        """
        Renders the main bar's background and then all navigation buttons.
        """
        # Draw the background of the entire main bar
        self.object_layer_render.draw_rectangle(
            0,  # X starts from left edge of screen
            self.y_position,  # Y starts at the calculated position for the bar
            self.screen_width,  # Bar spans full width
            self.height,  # Bar has its calculated height
            self.ui_modal_background_color,  # Use the default background color
        )

        for button in self.navigation_buttons:
            button.render(self.object_layer_render, mouse_x, mouse_y)

    def handle_clicks(
        self, mouse_x: int, mouse_y: int, is_mouse_button_pressed: bool
    ) -> str | None:
        """
        Handles clicks on the navigation buttons.
        Returns the route path if a button was clicked, otherwise None.
        """
        for button in self.navigation_buttons:
            if button.check_click(mouse_x, mouse_y, is_mouse_button_pressed):
                route_path = button.data_to_pass.get("route_path")
                if route_path:
                    logging.info(f"Navigation button clicked: {route_path}")
                    return route_path
        return None

    def update_screen_dimensions(self, new_screen_width: int, new_screen_height: int):
        """
        Updates the screen dimensions for the main bar and all navigation buttons and repositions them.
        """
        self.screen_width = new_screen_width
        self.screen_height = new_screen_height

        # Recalculate bar's height and position based on new screen height
        self.height = self.btn_modal_height + self.btn_modal_padding_bottom * 2
        self.y_position = self.screen_height - self.height

        # Update position for all navigation buttons
        for i, modal in enumerate(self.navigation_buttons):
            modal.screen_width = new_screen_width
            modal.screen_height = new_screen_height  # Pass full screen height for consistency, though buttons are relative to bar
            horizontal_offset = i * (
                self.btn_modal_width + self.btn_modal_padding_right
            )
            # Recalculate button's X and Y relative to the new screen dimensions and bar position
            modal.x = (
                self.screen_width
                - modal.width
                - modal.padding_right  # Original padding_right from constructor
                - horizontal_offset
            )
            # The button's y is based on the bar's y_position plus its own padding_bottom
            modal.y = self.y_position + self.btn_modal_padding_bottom
