import logging
from raylibpy import Color, Vector2
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

    def __init__(self, object_layer_render_instance: ObjectLayerRender):
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

        # Ensure minimum dimensions
        if self.map_viewport_width < 100:
            self.map_viewport_width = 100
        if self.map_viewport_height < 100:
            self.map_viewport_height = 100

        # Calculate map viewport position to center it within the available content area
        self.map_viewport_x = x + (width - self.map_viewport_width) // 2
        self.map_viewport_y = (
            content_area_y_start + (content_area_height - self.map_viewport_height) // 2
        )

        # Draw the map viewport background
        self.object_layer_render.draw_rectangle(
            self.map_viewport_x,
            self.map_viewport_y,
            self.map_viewport_width,
            self.map_viewport_height,
            Color(20, 20, 20, 255),  # Dark background for the map
        )

        # Draw a border for the map viewport
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

        # Draw zoom level indicator
        zoom_text = f"Zoom: {int(self.current_zoom_factor * 100)}%"
        zoom_text_width = self.object_layer_render.measure_text(
            zoom_text, UI_FONT_SIZE - 4
        )
        self.object_layer_render.draw_text(
            zoom_text,
            self.map_viewport_x + self.map_viewport_width - zoom_text_width - 5,
            self.map_viewport_y + self.map_viewport_height + 5,
            UI_FONT_SIZE - 4,
            Color(*UI_TEXT_COLOR_PRIMARY),
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
        return False

    def reset_view(self):
        """
        Resets the view state. For the map view, resets the zoom level.
        """
        self.title_text = "World Map"
        self.current_zoom_index = 0  # Reset to default zoom (1.0x)
        logging.info("Map view reset.")
