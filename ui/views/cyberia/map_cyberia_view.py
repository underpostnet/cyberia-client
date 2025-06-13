import logging
from raylibpy import Color, Vector2, Rectangle, check_collision_point_rec
from config import (
    UI_FONT_SIZE,
    UI_TEXT_COLOR_PRIMARY,
    UI_TEXT_COLOR_SHADING,
    MAP_VIEWPORT_WIDTH,
    MAP_VIEWPORT_HEIGHT,
    WORLD_WIDTH,
    WORLD_HEIGHT,
    NETWORK_OBJECT_SIZE,  # Used for calculating map cell sizes based on world units
    MAP_ZOOM_LEVELS,
)
from object_layer.object_layer_render import ObjectLayerRender
from network_state.network_state import NetworkState  # Import for type hinting
from network_state.network_object import NetworkObject  # Import for type hinting
from network_state.network_state_proxy import NetworkStateProxy # For sending channel change requests
from raylibpy import get_mouse_wheel_move  # Import for mouse wheel input

logging.basicConfig(
    level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s"
)


class MapCyberiaView:
    """
    Manages the display and interaction for the map interface.
    This view renders a detailed map of the world, centered on the player,
    with different zoom levels.
    """

    def __init__(
        self,
        object_layer_render_instance: ObjectLayerRender,
        network_proxy: NetworkStateProxy, # Added network_proxy
    ):
        self.object_layer_render = object_layer_render_instance
        self.title_text = "World Map"

        self.map_viewport_width = MAP_VIEWPORT_WIDTH
        self.map_viewport_height = MAP_VIEWPORT_HEIGHT

        self.zoom_levels = MAP_ZOOM_LEVELS
        self.current_zoom_index = (
            1  # Default to 1.0x zoom (first element in new MAP_ZOOM_LEVELS)
        )

        self.map_viewport_x = 0
        self.map_viewport_y = 0
        self.network_proxy = network_proxy # Store the proxy instance

        # Channel switching buttons
        self.channel_buttons: list[dict] = [] # To store button rects and IDs

    @property
    def current_zoom_factor(self) -> float:
        """Returns the current zoom factor based on the selected index."""
        if not self.zoom_levels:
            return 1.0  # Default to 1 if no zoom levels are defined
        return self.zoom_levels[self.current_zoom_index]

    def _clip_to_viewport(
        self, draw_x: int, draw_y: int, draw_width: int, draw_height: int
    ) -> tuple[int, int, int, int]:
        """
        Clips the given drawing rectangle to stay within the map viewport boundaries.
        Returns the clipped (x, y, width, height).
        """
        viewport_left = self.map_viewport_x
        viewport_top = self.map_viewport_y
        viewport_right = self.map_viewport_x + self.map_viewport_width
        viewport_bottom = self.map_viewport_y + self.map_viewport_height

        # Calculate intersection
        clipped_x = max(draw_x, viewport_left)
        clipped_y = max(draw_y, viewport_top)
        clipped_right = min(draw_x + draw_width, viewport_right)
        clipped_bottom = min(draw_y + draw_height, viewport_bottom)

        clipped_width = max(0, clipped_right - clipped_x)
        clipped_height = max(0, clipped_bottom - clipped_y)

        return clipped_x, clipped_y, clipped_width, clipped_height

    def render_content(
        self,
        modal_component,
        x: int,
        y: int,
        width: int,
        height: int,
        mouse_x: int,
        mouse_y: int,
        is_mouse_button_pressed: bool,
    ):
        """
        Renders the map view content: a centered map showing network objects.

        Args:
            modal_component: The parent ModalCoreComponent instance.
            x, y, width, height: Dimensions of the modal container.
            mouse_x, mouse_y: Current mouse coordinates.
            is_mouse_button_pressed: True if the mouse button is pressed.
        """
        # Update modal title
        modal_component.set_title(self.title_text)

        # Draw the title for the map view
        title_font_size = UI_FONT_SIZE + 2
        title_text_width = self.object_layer_render.measure_text(
            self.title_text, title_font_size
        )
        title_x = x + (width - title_text_width) // 2
        title_y = y + 20

        self.object_layer_render.draw_text(
            self.title_text,
            title_x + 1,
            title_y + 1,
            title_font_size,
            Color(*UI_TEXT_COLOR_SHADING),
        )
        self.object_layer_render.draw_text(
            self.title_text,
            title_x,
            title_y,
            title_font_size,
            Color(*UI_TEXT_COLOR_PRIMARY),
        )

        # --- Map Viewport Positioning ---
        # Account for the title area when centering vertically for the map viewport
        content_area_y_start = y + title_font_size + 30
        content_area_height = height - (content_area_y_start - y)

        # Adjust map viewport width and height based on the modal's current dimensions
        # This will allow the map to expand when the modal is maximized
        self.map_viewport_width = width - 40  # Some padding from the modal edges
        self.map_viewport_height = (
            content_area_height - 40
        )  # Some padding from the top/bottom

        # Ensure map viewport dimensions are not negative.
        # If the modal is too small, the viewport can be zero or very small.
        self.map_viewport_width = max(0, self.map_viewport_width)
        self.map_viewport_height = max(0, self.map_viewport_height)

        # Calculate map viewport position to center it within the available content area
        self.map_viewport_x = x + (width - self.map_viewport_width) // 2
        self.map_viewport_y = (
            content_area_y_start + (content_area_height - self.map_viewport_height) // 2
        )

        # If the viewport is too small, skip rendering map details and zoom handling.
        # Title is already drawn. Modal background is handled by ModalCoreComponent.
        MIN_PRACTICAL_VIEWPORT_DIM = 10  # pixels
        if (
            self.map_viewport_width < MIN_PRACTICAL_VIEWPORT_DIM
            or self.map_viewport_height < MIN_PRACTICAL_VIEWPORT_DIM
        ):
            return

        # Draw the map viewport background and border if dimensions are positive
        # (guaranteed if we passed the MIN_PRACTICAL_VIEWPORT_DIM check)
        self.object_layer_render.draw_rectangle(
            self.map_viewport_x,
            self.map_viewport_y,
            self.map_viewport_width,
            self.map_viewport_height,
            Color(20, 20, 20, 255),  # Dark background for the map
        )
        self.object_layer_render.draw_rectangle_lines(
            self.map_viewport_x,
            self.map_viewport_y,
            self.map_viewport_width,
            self.map_viewport_height,
            Color(100, 100, 100, 255),  # Gray border
        )

        # --- Map Rendering Logic ---
        network_state: NetworkState = modal_component.data_to_pass.get("network_state")
        my_network_object: NetworkObject = modal_component.data_to_pass.get(
            "my_network_object"
        )

        if not network_state or not my_network_object:
            self.object_layer_render.draw_text(
                "Map data not available.",
                self.map_viewport_x + 10,
                self.map_viewport_y + 10,
                UI_FONT_SIZE - 2,
                Color(*UI_TEXT_COLOR_PRIMARY),
            )
            return

        with network_state.lock:
            # Calculate the world area visible in the map viewport
            # The player should be centered in the map viewport using its smoothed position
            smoothed_player_pos = self.object_layer_render.get_smoothed_object_position(
                my_network_object.obj_id
            )
            player_world_center_x = (
                smoothed_player_pos.x if smoothed_player_pos else my_network_object.x
            ) + NETWORK_OBJECT_SIZE / 2
            player_world_center_y = (
                smoothed_player_pos.y if smoothed_player_pos else my_network_object.y
            ) + NETWORK_OBJECT_SIZE / 2

            # Calculate world dimensions of the visible area based on current zoom
            world_view_width_at_zoom = (
                self.map_viewport_width / self.current_zoom_factor
            )
            world_view_height_at_zoom = (
                self.map_viewport_height / self.current_zoom_factor
            )

            # Calculate the top-left world coordinate of the visible area, centering the player
            world_render_x_start = player_world_center_x - (
                world_view_width_at_zoom / 2
            )
            world_render_y_start = player_world_center_y - (
                world_view_height_at_zoom / 2
            )

            # Clamp world_render_x_start and world_render_y_start to world boundaries
            # This ensures the map rendering never goes out of view
            world_render_x_start = max(
                0, min(world_render_x_start, WORLD_WIDTH - world_view_width_at_zoom)
            )
            world_render_y_start = max(
                0, min(world_render_y_start, WORLD_HEIGHT - world_view_height_at_zoom)
            )

            # Calculate the size of each grid cell on the map based on zoom
            # This ensures cells appear adjacent and dynamically adjust.
            scaled_map_cell_size = int(NETWORK_OBJECT_SIZE * self.current_zoom_factor)
            if scaled_map_cell_size < 1:
                scaled_map_cell_size = 1  # Ensure minimum 1px dot for very low zoom

            # Prepare a list to draw player later, so it's always on top
            player_map_coords = None

            # Iterate through all network objects
            for obj_id, obj in network_state.network_objects.items():
                # Use smoothed position for drawing on map if available, otherwise fall back to raw
                smoothed_obj_pos = (
                    self.object_layer_render.get_smoothed_object_position(obj_id)
                )
                obj_world_x = smoothed_obj_pos.x if smoothed_obj_pos else obj.x
                obj_world_y = smoothed_obj_pos.y if smoothed_obj_pos else obj.y

                # Convert this object's world coordinates to map viewport coordinates
                map_x_relative = obj_world_x - world_render_x_start
                map_y_relative = obj_world_y - world_render_y_start

                map_x_on_viewport = (
                    int(map_x_relative * self.current_zoom_factor) + self.map_viewport_x
                )
                map_y_on_viewport = (
                    int(map_y_relative * self.current_zoom_factor) + self.map_viewport_y
                )

                # If the object is outside the current map viewport, skip it
                if not (
                    map_x_on_viewport + scaled_map_cell_size > self.map_viewport_x
                    and map_x_on_viewport
                    < self.map_viewport_x + self.map_viewport_width
                    and map_y_on_viewport + scaled_map_cell_size > self.map_viewport_y
                    and map_y_on_viewport
                    < self.map_viewport_y + self.map_viewport_height
                ):
                    continue

                # Draw the object if it's NOT the player
                if obj.obj_id != my_network_object.obj_id:
                    color_to_draw = obj.color

                    # Clip the rectangle to the viewport for drawing
                    clipped_x, clipped_y, clipped_width, clipped_height = (
                        self._clip_to_viewport(
                            map_x_on_viewport,
                            map_y_on_viewport,
                            scaled_map_cell_size,
                            scaled_map_cell_size,
                        )
                    )

                    self.object_layer_render.draw_rectangle(
                        clipped_x,
                        clipped_y,
                        clipped_width,
                        clipped_height,
                        color_to_draw,
                    )
                    # Draw border for non-player objects to make cells noticeable
                    self.object_layer_render.draw_rectangle_lines(
                        clipped_x,
                        clipped_y,
                        clipped_width,
                        clipped_height,
                        Color(60, 60, 60, 255),  # Darker gray border
                    )
                else:
                    # Store player's coordinates to draw it last
                    player_map_coords = (
                        map_x_on_viewport,
                        map_y_on_viewport,
                        scaled_map_cell_size,
                    )

            # --- Draw Player Last (Always on Top) ---
            if player_map_coords:
                player_map_x, player_map_y, player_scaled_size = player_map_coords

                # Clip the player rectangle to the viewport for drawing
                clipped_x, clipped_y, clipped_width, clipped_height = (
                    self._clip_to_viewport(
                        player_map_x,
                        player_map_y,
                        player_scaled_size,
                        player_scaled_size,
                    )
                )

                self.object_layer_render.draw_rectangle(
                    clipped_x,
                    clipped_y,
                    clipped_width,
                    clipped_height,
                    Color(255, 255, 0, 255),  # Yellow for player
                )
                # Draw border for player as well
                self.object_layer_render.draw_rectangle_lines(
                    clipped_x,
                    clipped_y,
                    clipped_width,
                    clipped_height,
                    Color(60, 60, 60, 255),  # Darker gray border
                )

        # --- Zoom Controls (Text and Buttons) ---
        button_size = 40  # Set button size to 40x40px
        button_padding = 5
        control_area_padding_from_bottom_right = 10 # Overall padding for the control group
        controls_font_size = UI_FONT_SIZE  # Adjusted font size for controls text

        # Define colors for button states
        enabled_button_text_color = Color(*UI_TEXT_COLOR_PRIMARY)
        enabled_button_bg_color = Color(70, 70, 70, 220)  # Semi-transparent dark gray
        hover_button_bg_color = Color(
            100, 100, 100, 220
        )  # Background color for hover state
        button_border_color = Color(180, 180, 180, 255)
        disabled_button_text_color = Color(120, 120, 120, 255)  # Grayed out text
        disabled_button_bg_color = Color(50, 50, 50, 150)  # Darker, more transparent bg

        # Calculate positions (from right to left for layout)
        # Zoom Out Button (-)
        zoom_out_btn_x = (
            self.map_viewport_x + self.map_viewport_width - control_area_padding_from_bottom_right - button_size
        )
        zoom_out_btn_y = (
            self.map_viewport_y
            + self.map_viewport_height
            - button_size
            - control_area_padding_from_bottom_right
        )
        zoom_out_button_rect = Rectangle(
            zoom_out_btn_x, zoom_out_btn_y, button_size, button_size
        )

        # Zoom In Button (+)
        zoom_in_btn_x = zoom_out_btn_x - button_padding - button_size
        zoom_in_btn_y = zoom_out_btn_y
        zoom_in_button_rect = Rectangle(
            zoom_in_btn_x, zoom_in_btn_y, button_size, button_size
        )

        # Zoom Level Text
        zoom_level_text_content = f"Zoom: {int(self.current_zoom_factor * 100)}%"
        zoom_level_text_width = self.object_layer_render.measure_text(
            zoom_level_text_content, controls_font_size
        )
        zoom_level_text_x = zoom_in_btn_x - button_padding - zoom_level_text_width
        zoom_level_text_y = (
            zoom_in_btn_y + (button_size - controls_font_size) // 2
        )  # Vertically center text with buttons

        # --- Channel Switch Buttons ---
        # Position channel buttons to the left of the zoom controls or at the top-left of viewport
        self.channel_buttons = [] # Clear previous button definitions
        channel_button_y = self.map_viewport_y + control_area_padding_from_bottom_right
        channel_button_x_start = self.map_viewport_x + control_area_padding_from_bottom_right
        
        # Example channel IDs - these should ideally come from a config or be discoverable
        available_channels = ["channel_alpha", "channel_beta"]
        current_channel_id = modal_component.data_to_pass.get("current_channel_id", "channel_alpha")

        for i, chan_id in enumerate(available_channels):
            btn_text = f"Ch: {chan_id.split('_')[-1].capitalize()}" # e.g., "Ch: Alpha"
            btn_text_width = self.object_layer_render.measure_text(btn_text, controls_font_size -2)
            chan_btn_width = btn_text_width + 2 * button_padding # Dynamic width based on text
            chan_btn_height = button_size - 10 # Slightly smaller height

            chan_btn_x = channel_button_x_start + sum(b['rect'].width + button_padding for b in self.channel_buttons)
            
            # Check if this button fits before adding
            if chan_btn_x + chan_btn_width > self.map_viewport_x + self.map_viewport_width - control_area_padding_from_bottom_right:
                break # Stop adding channel buttons if they overflow

            channel_button_rect = Rectangle(chan_btn_x, channel_button_y, chan_btn_width, chan_btn_height)
            self.channel_buttons.append({"id": chan_id, "text": btn_text, "rect": channel_button_rect})

            is_current_channel = chan_id == current_channel_id
            
            actual_chan_btn_bg_color = enabled_button_bg_color
            if is_current_channel:
                actual_chan_btn_bg_color = Color(0,100,0,220) # Dark green for current channel
            elif check_collision_point_rec(Vector2(mouse_x, mouse_y), channel_button_rect):
                actual_chan_btn_bg_color = hover_button_bg_color

            self.object_layer_render.draw_rectangle_rec(channel_button_rect, actual_chan_btn_bg_color)
            self.object_layer_render.draw_rectangle_lines_ex(channel_button_rect, 1, button_border_color)
            self.object_layer_render.draw_text(
                btn_text,
                int(chan_btn_x + (chan_btn_width - btn_text_width) // 2),
                int(channel_button_y + (chan_btn_height - (controls_font_size - 2)) // 2),
                controls_font_size - 2,
                enabled_button_text_color
            )


        # Check if there's enough space to draw controls
        min_controls_width_needed = (
            zoom_level_text_width + (button_padding + button_size) * 2 + button_padding
        )
        min_controls_height_needed = button_size + button_padding * 2



        if (
            self.map_viewport_width >= min_controls_width_needed
            and self.map_viewport_height >= min_controls_height_needed
            and zoom_level_text_x >= self.map_viewport_x + button_padding
        ):  # Ensure text doesn't overflow left

            can_zoom_out = self.current_zoom_index > 0
            can_zoom_in = self.current_zoom_index < len(self.zoom_levels) - 1

            # Create mouse position vector for hover and click checks
            mouse_pos_vec = Vector2(mouse_x, mouse_y)

            # Draw Zoom Out Button
            actual_zoom_out_bg_color = disabled_button_bg_color
            actual_zoom_out_text_color = disabled_button_text_color
            if can_zoom_out:
                actual_zoom_out_text_color = enabled_button_text_color
                if check_collision_point_rec(mouse_pos_vec, zoom_out_button_rect):
                    actual_zoom_out_bg_color = hover_button_bg_color
                else:
                    actual_zoom_out_bg_color = enabled_button_bg_color

            self.object_layer_render.draw_rectangle(
                int(zoom_out_button_rect.x),
                int(zoom_out_button_rect.y),
                int(zoom_out_button_rect.width),
                int(zoom_out_button_rect.height),
                actual_zoom_out_bg_color,
            )
            self.object_layer_render.draw_rectangle_lines(
                int(zoom_out_button_rect.x),
                int(zoom_out_button_rect.y),
                int(zoom_out_button_rect.width),
                int(zoom_out_button_rect.height),
                button_border_color,
            )
            minus_text = "-"
            minus_text_width = self.object_layer_render.measure_text(
                minus_text, controls_font_size
            )
            self.object_layer_render.draw_text(
                minus_text,
                int(zoom_out_btn_x + (button_size - minus_text_width) // 2),
                int(zoom_out_btn_y + (button_size - controls_font_size) // 2),
                controls_font_size,
                actual_zoom_out_text_color,
            )

            # Draw Zoom In Button
            actual_zoom_in_bg_color = disabled_button_bg_color
            actual_zoom_in_text_color = disabled_button_text_color
            if can_zoom_in:
                actual_zoom_in_text_color = enabled_button_text_color
                if check_collision_point_rec(mouse_pos_vec, zoom_in_button_rect):
                    actual_zoom_in_bg_color = hover_button_bg_color
                else:
                    actual_zoom_in_bg_color = enabled_button_bg_color

            self.object_layer_render.draw_rectangle(
                int(zoom_in_button_rect.x),
                int(zoom_in_button_rect.y),
                int(zoom_in_button_rect.width),
                int(zoom_in_button_rect.height),
                actual_zoom_in_bg_color,
            )
            self.object_layer_render.draw_rectangle_lines(
                int(zoom_in_button_rect.x),
                int(zoom_in_button_rect.y),
                int(zoom_in_button_rect.width),
                int(zoom_in_button_rect.height),
                button_border_color,
            )
            plus_text = "+"
            plus_text_width = self.object_layer_render.measure_text(
                plus_text, controls_font_size
            )
            self.object_layer_render.draw_text(
                plus_text,
                int(zoom_in_btn_x + (button_size - plus_text_width) // 2),
                int(zoom_in_btn_y + (button_size - controls_font_size) // 2),
                controls_font_size,
                actual_zoom_in_text_color,
            )

            # Draw Zoom Level Text
            self.object_layer_render.draw_text(
                zoom_level_text_content,
                int(zoom_level_text_x),
                int(zoom_level_text_y),
                controls_font_size,
                Color(*UI_TEXT_COLOR_PRIMARY),
            )

            # Handle button clicks
            if is_mouse_button_pressed:
                # Check mouse is within map viewport before processing button clicks
                if check_collision_point_rec(
                    mouse_pos_vec,
                    Rectangle(
                        self.map_viewport_x,
                        self.map_viewport_y,
                        self.map_viewport_width,
                        self.map_viewport_height,
                    ),
                ):
                    if can_zoom_in and check_collision_point_rec(
                        mouse_pos_vec, zoom_in_button_rect
                    ):
                        new_zoom_index = min(
                            self.current_zoom_index + 1, len(self.zoom_levels) - 1
                        )
                        if new_zoom_index != self.current_zoom_index:
                            self.current_zoom_index = new_zoom_index
                            logging.info(
                                f"Map zoom level changed to {self.current_zoom_factor} via [+] button"
                            )
                    elif can_zoom_out and check_collision_point_rec(
                        mouse_pos_vec, zoom_out_button_rect
                    ):
                        new_zoom_index = max(self.current_zoom_index - 1, 0)
                        if new_zoom_index != self.current_zoom_index:
                            self.current_zoom_index = new_zoom_index
                            logging.info(
                                f"Map zoom level changed to {self.current_zoom_factor} via [-] button"
                            )
        # Handle zoom input
        self._handle_zoom_input(mouse_x, mouse_y)

    def _handle_zoom_input(self, mouse_x: int, mouse_y: int):
        """
        Handles mouse wheel input for zooming the map.
        Only allows zooming if the mouse is over the map viewport.
        """
        is_mouse_over_map_viewport = (
            mouse_x >= self.map_viewport_x
            and mouse_x <= (self.map_viewport_x + self.map_viewport_width)
            and mouse_y >= self.map_viewport_y
            and mouse_y <= (self.map_viewport_y + self.map_viewport_height)
        )

        if is_mouse_over_map_viewport:
            wheel_move = get_mouse_wheel_move()
            if wheel_move != 0:
                new_zoom_index = self.current_zoom_index
                if wheel_move > 0:  # Zoom in
                    new_zoom_index = min(new_zoom_index + 1, len(self.zoom_levels) - 1)
                elif wheel_move < 0:  # Zoom out
                    new_zoom_index = max(new_zoom_index - 1, 0)

                if new_zoom_index != self.current_zoom_index:
                    self.current_zoom_index = new_zoom_index
                    logging.info(
                        f"Map zoom level changed to {self.current_zoom_factor}"
                    )

    def handle_item_clicks(
        self,
        offset_x: int,
        offset_y: int,
        container_width: int,
        container_height: int,
        mouse_x: int,
        mouse_y: int,
        is_mouse_button_pressed: bool,
    ) -> bool:
        """
        Handles clicks within the map modal.
        This view has no interactive elements that respond to simple clicks,
        as zoom is controlled by mouse wheel.
        """
        if not is_mouse_button_pressed:
            return False

        # Check channel button clicks
        mouse_pos_vec = Vector2(mouse_x, mouse_y)
        if check_collision_point_rec(mouse_pos_vec, Rectangle(self.map_viewport_x, self.map_viewport_y, self.map_viewport_width, self.map_viewport_height)):
            for btn_data in self.channel_buttons:
                if check_collision_point_rec(mouse_pos_vec, btn_data["rect"]):
                    channel_to_switch = btn_data["id"]
                    logging.info(f"Map View: Channel button '{channel_to_switch}' clicked.")
                    # Send message to proxy to change channel
                    if self.network_proxy:
                        self.network_proxy.send_client_message(
                            "client_change_channel_request",
                            {"channel_id": channel_to_switch}
                        )
                    return True # Click was handled
        return False # No map-specific click handled

    def reset_view(self):
        """
        Resets the view state. For the map view, resets the zoom level.
        """
        self.title_text = "World Map"
        self.current_zoom_index = 0  # Reset to default zoom (1.0x)
        logging.info("Map view reset.")
