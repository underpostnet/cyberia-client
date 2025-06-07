import logging
from typing import Callable, Dict, Any, List
from functools import partial

from raylibpy import Color

from ui.components.core.modal_core_component import ModalCoreComponent
from ui.components.core.texture_manager import TextureManager
from object_layer.object_layer_render import ObjectLayerRender  # Added missing import


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
        ui_modal_background_color: Color,
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

        # Calculate the height of the main bar
        self.height = self.btn_modal_height + self.btn_modal_padding_bottom

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
                padding_bottom=self.btn_modal_padding_bottom,
                padding_right=self.btn_modal_padding_right,
                horizontal_offset=horizontal_offset,
                background_color=self.ui_modal_background_color,
                icon_texture=icon_texture,
                title_text=route["name"],  # Name for the button
            )
            self.navigation_buttons.append(modal_btn)
            # Store a reference to the route in the button's data for click handling
            modal_btn.data_to_pass["route_path"] = route["path"]

    def render(self, mouse_x: int, mouse_y: int):
        """
        Renders all navigation buttons in the main bar.
        """
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
        Updates the screen dimensions for all navigation buttons and repositions them.
        """
        self.screen_width = new_screen_width
        self.screen_height = new_screen_height

        # Update position for all navigation buttons
        for i, modal in enumerate(self.navigation_buttons):
            modal.screen_width = new_screen_width
            modal.screen_height = new_screen_height
            horizontal_offset = i * (
                self.btn_modal_width + self.btn_modal_padding_right
            )
            modal.x = (
                self.screen_width
                - modal.width
                - modal.padding_right
                - horizontal_offset
            )
            modal.y = self.screen_height - modal.height - modal.padding_bottom
