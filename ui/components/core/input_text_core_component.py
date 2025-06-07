import logging
from raylibpy import (
    Color,
    Rectangle,
    get_clipboard_text,
    set_clipboard_text,
    is_key_pressed,
    is_key_down,
    get_char_pressed,
    KEY_BACKSPACE,
    KEY_LEFT_SHIFT,
    KEY_RIGHT_SHIFT,
    KEY_C,
    KEY_V,
    KEY_HOME,
    KEY_END,
    KEY_LEFT,
    KEY_RIGHT,
    KEY_DELETE,
    KEY_A,
)

logging.basicConfig(
    level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s"
)


class InputTextCoreComponent:
    """
    A UI component for text input, handling rendering, user input, and cursor blinking.
    """

    def __init__(
        self,
        object_layer_render_instance,
        x: int,
        y: int,
        width: int,
        height: int,
        font_size: int,
        text_color: Color,
        background_color: Color,
        border_color: Color,
        shading_color: Color,
        initial_text: str = "",
        max_length: int = 256,
        is_active: bool = False,
    ):
        """
        Initializes the InputTextCoreComponent.

        Args:
            object_layer_render_instance: An instance of ObjectLayerRender for drawing and text measurement.
            x, y, width, height: Dimensions and position of the input field.
            font_size: Font size for the text.
            text_color, background_color, border_color, shading_color: Colors for rendering.
            initial_text: The starting text in the input field.
            max_length: Maximum number of characters allowed.
            is_active: Initial active state of the input field.
        """
        self.object_layer_render = object_layer_render_instance
        self.x = x
        self.y = y
        self.width = width
        self.height = height
        self.font_size = font_size
        self.text_color = text_color
        self.background_color = background_color
        self.border_color = border_color
        self.shading_color = shading_color
        self.text = initial_text
        self.max_length = max_length
        self.is_active = is_active
        self.cursor_blink_timer = 0.0
        self.cursor_visible = True
        self.cursor_position = len(initial_text)  # Cursor at end of initial text

    def set_active(self, active: bool):
        """Sets the active state of the input field."""
        self.is_active = active
        self.cursor_blink_timer = 0.0  # Reset timer when activation changes
        self.cursor_visible = True  # Ensure cursor is visible when activated

    def update(
        self,
        dt: float,
        char_pressed: int | None,
        key_pressed: int | None,
        is_key_down_map: dict,
    ):
        """
        Updates the input text component, handling text input and cursor blinking.

        Args:
            dt: Delta time for animation.
            char_pressed: The character value of the key pressed in the current frame.
            key_pressed: The key code of the key pressed in the current frame.
            is_key_down_map: A dictionary representing currently held down keys.
        """
        if not self.is_active:
            return

        # Cursor blinking
        self.cursor_blink_timer += dt
        if self.cursor_blink_timer >= 0.5:  # Blink every 0.5 seconds
            self.cursor_visible = not self.cursor_visible
            self.cursor_blink_timer = 0.0

        # Handle character input
        if char_pressed and len(self.text) < self.max_length:
            # Append valid characters (e.g., alphanumeric, symbols, space)
            if 32 <= char_pressed <= 125:  # ASCII range for printable characters
                self.text = (
                    self.text[: self.cursor_position]
                    + chr(char_pressed)
                    + self.text[self.cursor_position :]
                )
                self.cursor_position += 1

        # Handle special keys
        if key_pressed:
            if key_pressed == KEY_BACKSPACE:
                if self.cursor_position > 0:
                    self.text = (
                        self.text[: self.cursor_position - 1]
                        + self.text[self.cursor_position :]
                    )
                    self.cursor_position -= 1
            elif key_pressed == KEY_DELETE:  # Directly use KEY_DELETE
                if self.cursor_position < len(self.text):
                    self.text = (
                        self.text[: self.cursor_position]
                        + self.text[self.cursor_position + 1 :]
                    )
            elif key_pressed == KEY_LEFT:  # Directly use KEY_LEFT
                self.cursor_position = max(0, self.cursor_position - 1)
            elif key_pressed == KEY_RIGHT:  # Directly use KEY_RIGHT
                self.cursor_position = min(len(self.text), self.cursor_position + 1)
            elif key_pressed == KEY_HOME:  # Directly use KEY_HOME
                self.cursor_position = 0
            elif key_pressed == KEY_END:  # Directly use KEY_END
                self.cursor_position = len(self.text)
            elif key_pressed == KEY_A and (
                is_key_down_map.get(KEY_LEFT_SHIFT)
                or is_key_down_map.get(KEY_RIGHT_SHIFT)
            ):
                # Ctrl+A (Select All) is not directly supported by raylibpy, usually requires custom logic.
                # For now, we'll skip implementing full select-all functionality.
                pass
            elif key_pressed == KEY_C and (
                is_key_down_map.get(KEY_LEFT_SHIFT)
                or is_key_down_map.get(KEY_RIGHT_SHIFT)
            ):
                # Ctrl+C (Copy)
                if self.text:
                    set_clipboard_text(self.text)
                    logging.info("Text copied to clipboard.")
            elif key_pressed == KEY_V and (
                is_key_down_map.get(KEY_LEFT_SHIFT)
                or is_key_down_map.get(KEY_RIGHT_SHIFT)
            ):
                # Ctrl+V (Paste)
                clipboard_text = get_clipboard_text()
                if clipboard_text:
                    new_text = (
                        self.text[: self.cursor_position]
                        + clipboard_text
                        + self.text[self.cursor_position :]
                    )
                    if len(new_text) <= self.max_length:
                        self.text = new_text
                        self.cursor_position += len(clipboard_text)
                    else:
                        logging.warning(
                            "Pasted text exceeds max length and was truncated or ignored."
                        )

    def render(self):
        """Renders the input text box and cursor."""
        # Draw background
        self.object_layer_render.draw_rectangle(
            self.x, self.y, self.width, self.height, self.background_color
        )
        # Draw border
        self.object_layer_render.draw_rectangle_lines(
            self.x, self.y, self.width, self.height, self.border_color
        )

        # Calculate text position (with padding)
        text_padding_x = 5
        text_padding_y = (self.height - self.font_size) // 2
        display_x = self.x + text_padding_x
        display_y = self.y + text_padding_y

        # Determine visible text based on cursor position and width
        rendered_text = self.text
        text_width_px = self.object_layer_render.measure_text(
            rendered_text, self.font_size
        )

        # Simple scrolling: if text is too long, show from the end
        max_text_width = self.width - 2 * text_padding_x
        if text_width_px > max_text_width:
            # Calculate how many characters fit
            temp_text = ""
            for i in range(len(self.text) - 1, -1, -1):
                test_char = self.text[i] + temp_text
                if (
                    self.object_layer_render.measure_text(test_char, self.font_size)
                    <= max_text_width
                ):
                    temp_text = test_char
                else:
                    break
            rendered_text = temp_text
            # Adjust display_x to right-align the visible part
            display_x = (
                self.x
                + self.width
                - text_padding_x
                - self.object_layer_render.measure_text(rendered_text, self.font_size)
            )
        else:
            display_x = self.x + text_padding_x

        # Draw text (with shading)
        self.object_layer_render.draw_text(
            rendered_text,
            display_x + 1,
            display_y + 1,
            self.font_size,
            self.shading_color,
        )
        self.object_layer_render.draw_text(
            rendered_text, display_x, display_y, self.font_size, self.text_color
        )

        # Draw cursor if active and visible
        if self.is_active and self.cursor_visible:
            # Calculate cursor X position based on text before cursor
            text_before_cursor = self.text[: self.cursor_position]
            cursor_x_offset = self.object_layer_render.measure_text(
                text_before_cursor, self.font_size
            )

            # Adjust cursor_x_offset if text is scrolled
            if text_width_px > max_text_width:
                # If the text is scrolled, the cursor position relative to the start of the *rendered* text changes.
                # This is a simplification; a full implementation would track scroll offset.
                # For now, if scrolled, assume cursor is at the far right of the visible text if it's past the beginning,
                # or at its normal position if it's within the visible portion.
                if self.cursor_position > len(self.text) - len(rendered_text):
                    # Cursor is in the visible (scrolled) part
                    cursor_x_offset = self.object_layer_render.measure_text(
                        self.text[
                            len(self.text) - len(rendered_text) : self.cursor_position
                        ],
                        self.font_size,
                    )
                else:
                    # Cursor is outside the visible left boundary, so don't draw it or clamp to left edge
                    cursor_x_offset = 0  # This hides it if it's off-screen left, or puts it at the start if it should be

            cursor_x = display_x + cursor_x_offset
            cursor_y = display_y
            cursor_height = self.font_size
            cursor_width = 2
            self.object_layer_render.draw_rectangle(
                cursor_x, cursor_y, cursor_width, cursor_height, self.text_color
            )

    def check_click(
        self, mouse_x: int, mouse_y: int, is_mouse_button_pressed: bool
    ) -> bool:
        """
        Checks if the input field was clicked.
        Returns True if clicked, False otherwise.
        """
        is_hovered = (
            mouse_x >= self.x
            and mouse_x <= (self.x + self.width)
            and mouse_y >= self.y
            and mouse_y <= (self.y + self.height)
        )
        if is_hovered and is_mouse_button_pressed:
            self.set_active(True)
            # When clicked, set cursor position based on click location
            # This is a rough estimation; precise positioning needs character width calculations
            relative_x = mouse_x - (self.x + 5)  # 5 for left padding
            temp_text = ""
            self.cursor_position = 0
            for i, char in enumerate(self.text):
                test_width = self.object_layer_render.measure_text(
                    temp_text + char, self.font_size
                )
                if test_width < relative_x:
                    temp_text += char
                    self.cursor_position = i + 1
                else:
                    break
            logging.debug(
                f"Input field clicked. Active: {self.is_active}, Cursor pos: {self.cursor_position}"
            )
            return True
        elif is_mouse_button_pressed and self.is_active and not is_hovered:
            self.set_active(False)
            logging.debug(f"Input field deactivated. Active: {self.is_active}")
            return False  # Clicked outside, so deactivate and don't count as handled by this component

        return False

    def get_text(self) -> str:
        """Returns the current text in the input field."""
        return self.text

    def set_text(self, new_text: str):
        """Sets the text of the input field, clamping to max_length."""
        self.text = new_text[: self.max_length]
        self.cursor_position = len(self.text)  # Move cursor to end of new text

    def is_currently_active(self) -> bool:
        """Returns true if the input field is currently active."""
        return self.is_active
