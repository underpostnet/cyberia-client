import raylibpy as rl


class ScrollbarCoreComponent:
    def __init__(
        self,
        x: float,
        y: float,
        width: float,
        height: float,
        track_color: rl.Color = rl.Color(50, 50, 50, 255),
        thumb_color: rl.Color = rl.Color(150, 150, 150, 255),
        min_thumb_height: float = 20.0,
    ):
        self.rect = rl.Rectangle(x, y, width, height)
        self.track_color = track_color
        self.thumb_color = thumb_color
        self.min_thumb_height = max(
            min_thumb_height, width
        )  # Thumb height shouldn't be less than its width

        self.thumb_rect = rl.Rectangle(
            x, y, width, 0
        )  # Initial height 0, calculated later
        self.is_dragging = False
        self._drag_offset_y = 0.0

        self.content_height: float = 0.0
        self.visible_height: float = height
        self.scroll_position: float = 0.0  # 0.0 (top) to 1.0 (bottom)
        self._update_thumb_size_and_position()  # Initial calculation

    def set_content_dimensions(self, content_height: float, visible_height: float):
        self.content_height = max(0.0, content_height)
        self.visible_height = max(
            1.0, visible_height
        )  # Avoid division by zero for ratios
        self.rect.height = (
            visible_height  # Scrollbar track height matches visible area for content
        )
        self._update_thumb_size_and_position()

    def set_scroll_position(self, position: float):
        self.scroll_position = max(0.0, min(1.0, position))
        self._update_thumb_size_and_position()

    def get_scroll_offset(self) -> float:
        if self.content_height <= self.visible_height:
            return 0.0
        max_scrollable_height = self.content_height - self.visible_height
        return self.scroll_position * max_scrollable_height

    def _update_thumb_size_and_position(self):
        self.thumb_rect.x = self.rect.x
        self.thumb_rect.width = self.rect.width

        if self.content_height <= self.visible_height or self.visible_height == 0:
            self.thumb_rect.height = (
                self.rect.height
            )  # Thumb fills track if no scroll needed
            self.thumb_rect.y = self.rect.y
            if (
                self.content_height <= self.visible_height
            ):  # Ensure scroll position is reset if content fits
                self.scroll_position = 0.0
        else:
            thumb_ratio = self.visible_height / self.content_height
            self.thumb_rect.height = max(
                self.min_thumb_height, self.rect.height * thumb_ratio
            )

            scrollable_track_height = self.rect.height - self.thumb_rect.height
            if (
                scrollable_track_height <= 0
            ):  # Thumb is as tall as or taller than track (should not happen if content_height > visible_height)
                self.thumb_rect.y = self.rect.y
                self.thumb_rect.height = (
                    self.rect.height
                )  # Clamp thumb height to track height
                self.scroll_position = 0.0
            else:
                self.thumb_rect.y = self.rect.y + (
                    self.scroll_position * scrollable_track_height
                )

    def handle_event(
        self,
        mouse_pos: rl.Vector2,
        is_mouse_btn_down: bool,
        is_mouse_btn_pressed: bool,
        mouse_wheel_move: float,
        scroll_wheel_sensitivity: float = 0.05,
    ) -> bool:
        event_consumed = False
        is_mouse_over_scrollbar_track = rl.check_collision_point_rec(
            mouse_pos, self.rect
        )
        # Thumb only exists if scrollable
        is_mouse_over_thumb = (
            self.content_height > self.visible_height
            and rl.check_collision_point_rec(mouse_pos, self.thumb_rect)
        )

        if self.is_dragging:
            if is_mouse_btn_down:
                new_thumb_y = mouse_pos.y - self._drag_offset_y
                new_thumb_y = max(
                    self.rect.y,
                    min(
                        self.rect.y + self.rect.height - self.thumb_rect.height,
                        new_thumb_y,
                    ),
                )
                self.thumb_rect.y = new_thumb_y

                scrollable_track_height = self.rect.height - self.thumb_rect.height
                if scrollable_track_height > 0:
                    self.scroll_position = (
                        self.thumb_rect.y - self.rect.y
                    ) / scrollable_track_height
                else:
                    self.scroll_position = 0.0
                event_consumed = True
            else:
                self.is_dragging = False
        elif is_mouse_btn_pressed:
            if is_mouse_over_thumb:
                self.is_dragging = True
                self._drag_offset_y = mouse_pos.y - self.thumb_rect.y
                event_consumed = True
            elif (
                is_mouse_over_scrollbar_track
                and self.content_height > self.visible_height
            ):  # Click on track
                clicked_y_in_track = mouse_pos.y - self.rect.y
                new_thumb_top_y_in_track = (
                    clicked_y_in_track - self.thumb_rect.height / 2.0
                )
                scrollable_track_height = self.rect.height - self.thumb_rect.height
                if scrollable_track_height > 0:
                    self.scroll_position = max(
                        0.0,
                        min(1.0, new_thumb_top_y_in_track / scrollable_track_height),
                    )
                else:
                    self.scroll_position = 0.0
                self._update_thumb_size_and_position()
                event_consumed = True

        if (
            is_mouse_over_scrollbar_track
            and mouse_wheel_move != 0.0
            and self.content_height > self.visible_height
        ):
            self.set_scroll_position(
                self.scroll_position - (mouse_wheel_move * scroll_wheel_sensitivity)
            )
            event_consumed = True

        return event_consumed

    def render(self):
        rl.draw_rectangle_rec(self.rect, self.track_color)
        if self.content_height > self.visible_height:
            rl.draw_rectangle_rec(self.thumb_rect, self.thumb_color)
