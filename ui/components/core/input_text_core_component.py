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
    KEY_LEFT_CONTROL,
    KEY_RIGHT_CONTROL,
    KEY_C,
    KEY_V,
    KEY_X,
    KEY_HOME,
    KEY_END,
    KEY_LEFT,
    KEY_RIGHT,
    KEY_DELETE,
    KEY_A,
    MOUSE_BUTTON_LEFT,
)

logging.basicConfig(
    level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s"
)


class InputTextCoreComponent:
    """
    A UI component for text input, handling rendering, user input, and cursor blinking.
    Includes text selection, visual highlighting, and Ctrl+A/C/V/X functionality.
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

        # Selection specific attributes
        self.selection_start: int | None = (
            None  # Index of the start of the selection (inclusive)
        )
        self.selection_end: int | None = (
            None  # Index of the end of the selection (exclusive)
        )
        self.is_dragging_selection: bool = False
        self.selection_color: Color = Color(
            50, 150, 200, 100
        )  # Light blue with transparency

        # Internal rendering properties, updated during render()
        # Initial values; these will be properly set in render() based on scrolling
        self._display_x = self.x + 5
        self._text_offset_char_idx = 0

    def set_active(self, active: bool):
        """Sets the active state of the input field."""
        self.is_active = active
        self.cursor_blink_timer = 0.0  # Reset timer when activation changes
        self.cursor_visible = True  # Ensure cursor is visible when activated
        if not active:
            self.clear_selection()

    def clear_selection(self):
        """Clears any active text selection."""
        self.selection_start = None
        self.selection_end = None
        self.is_dragging_selection = False

    def get_char_index_from_x(self, mouse_x: int) -> int:
        """
        Calculates the character index closest to a given mouse X coordinate
        within the input field's text area, considering the current scroll offset.
        """
        # Calculate relative X within the *visible* text area
        # _display_x accounts for padding and scrolling
        relative_x = mouse_x - self._display_x

        current_width = 0
        char_index_in_rendered_text = 0

        # Determine the subset of text that is currently rendered based on _text_offset_char_idx
        rendered_text_subset = self.text[self._text_offset_char_idx :]

        for i, char in enumerate(rendered_text_subset):
            char_width = self.object_layer_render.measure_text(char, self.font_size)
            if current_width + char_width / 2 > relative_x:
                break
            current_width += char_width
            char_index_in_rendered_text = i + 1

        # Convert back to index in the full text
        return self._text_offset_char_idx + char_index_in_rendered_text

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

        # Check for Ctrl key (Left or Right Control)
        is_control_down = is_key_down_map.get(
            KEY_LEFT_CONTROL, False
        ) or is_key_down_map.get(KEY_RIGHT_CONTROL, False)
        # Check for Shift key (Left or Right Shift) for selection extension
        is_shift_down = is_key_down_map.get(
            KEY_LEFT_SHIFT, False
        ) or is_key_down_map.get(KEY_RIGHT_SHIFT, False)

        # Handle character input
        if char_pressed:
            # If there's a selection, replace it with the new character
            if (
                self.selection_start is not None
                and self.selection_end is not None
                and self.selection_start != self.selection_end
            ):
                start_idx = min(self.selection_start, self.selection_end)
                end_idx = max(self.selection_start, self.selection_end)
                self.text = (
                    self.text[:start_idx] + chr(char_pressed) + self.text[end_idx:]
                )
                self.cursor_position = start_idx + 1
                self.clear_selection()
            elif len(self.text) < self.max_length:
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
            # Determine if a selection is active and valid (non-empty)
            has_active_selection = (
                self.selection_start is not None
                and self.selection_end is not None
                and self.selection_start != self.selection_end
            )

            # Helper to get normalized selection indices
            def get_normalized_selection():
                if has_active_selection:
                    return min(self.selection_start, self.selection_end), max(
                        self.selection_start, self.selection_end
                    )
                return None, None

            if key_pressed == KEY_BACKSPACE:
                # If there's a selection, delete the selected text
                if has_active_selection:
                    start_idx, end_idx = get_normalized_selection()
                    self.text = self.text[:start_idx] + self.text[end_idx:]
                    self.cursor_position = start_idx
                    self.clear_selection()
                elif self.cursor_position > 0:
                    self.text = (
                        self.text[: self.cursor_position - 1]
                        + self.text[self.cursor_position :]
                    )
                    self.cursor_position -= 1
            elif key_pressed == KEY_DELETE:
                # If there's a selection, delete the selected text
                if has_active_selection:
                    start_idx, end_idx = get_normalized_selection()
                    self.text = self.text[:start_idx] + self.text[end_idx:]
                    self.cursor_position = start_idx
                    self.clear_selection()
                elif self.cursor_position < len(self.text):
                    self.text = (
                        self.text[: self.cursor_position]
                        + self.text[self.cursor_position + 1 :]
                    )
            elif key_pressed == KEY_LEFT:
                new_cursor_pos = max(0, self.cursor_position - 1)
                if is_shift_down:
                    if (
                        self.selection_start is None
                    ):  # If starting a new selection with Shift
                        self.selection_start = self.cursor_position
                    self.selection_end = new_cursor_pos
                else:  # No shift, clear selection and move cursor
                    self.clear_selection()
                self.cursor_position = new_cursor_pos
            elif key_pressed == KEY_RIGHT:
                new_cursor_pos = min(len(self.text), self.cursor_position + 1)
                if is_shift_down:
                    if (
                        self.selection_start is None
                    ):  # If starting a new selection with Shift
                        self.selection_start = self.cursor_position
                    self.selection_end = new_cursor_pos
                else:  # No shift, clear selection and move cursor
                    self.clear_selection()
                self.cursor_position = new_cursor_pos
            elif key_pressed == KEY_HOME:
                new_cursor_pos = 0
                if is_shift_down:
                    if (
                        self.selection_start is None
                    ):  # If starting a new selection with Shift
                        self.selection_start = self.cursor_position
                    self.selection_end = new_cursor_pos
                else:  # No shift, clear selection and move cursor
                    self.clear_selection()
                self.cursor_position = new_cursor_pos
            elif key_pressed == KEY_END:
                new_cursor_pos = len(self.text)
                if is_shift_down:
                    if (
                        self.selection_start is None
                    ):  # If starting a new selection with Shift
                        self.selection_start = self.cursor_position
                    self.selection_end = new_cursor_pos
                else:  # No shift, clear selection and move cursor
                    self.clear_selection()
                self.cursor_position = new_cursor_pos
            elif key_pressed == KEY_A and is_control_down:  # Ctrl+A (Select All)
                self.selection_start = 0
                self.selection_end = len(self.text)
                self.cursor_position = len(
                    self.text
                )  # Move cursor to end of selected text
                logging.info("Select All triggered.")
            elif key_pressed == KEY_C and is_control_down:  # Ctrl+C (Copy)
                if has_active_selection:
                    start_idx, end_idx = get_normalized_selection()
                    selected_text = self.text[start_idx:end_idx]
                    set_clipboard_text(selected_text)
                    logging.info(
                        f"Selected text copied to clipboard: '{selected_text}'"
                    )
                elif self.text:  # If no selection, copy all text
                    set_clipboard_text(self.text)
                    logging.info("All text copied to clipboard.")
            elif key_pressed == KEY_X and is_control_down:  # Ctrl+X (Cut)
                if has_active_selection:
                    start_idx, end_idx = get_normalized_selection()
                    selected_text = self.text[start_idx:end_idx]
                    set_clipboard_text(selected_text)

                    # Delete the selected text
                    self.text = self.text[:start_idx] + self.text[end_idx:]
                    self.cursor_position = start_idx
                    self.clear_selection()
                    logging.info(f"Selected text cut to clipboard: '{selected_text}'")
                elif self.text:  # If no selection, cut all text
                    set_clipboard_text(self.text)
                    self.text = ""  # Clear the entire text
                    self.cursor_position = 0
                    self.clear_selection()
                    logging.info("All text cut to clipboard.")
            elif key_pressed == KEY_V and is_control_down:  # Ctrl+V (Paste)
                clipboard_text = get_clipboard_text()
                if clipboard_text:
                    if has_active_selection:
                        # Replace selected text with clipboard content
                        start_idx, end_idx = get_normalized_selection()
                        new_text = (
                            self.text[:start_idx] + clipboard_text + self.text[end_idx:]
                        )
                        self.cursor_position = start_idx + len(clipboard_text)
                        self.clear_selection()
                    else:
                        # Insert clipboard content at cursor position
                        new_text = (
                            self.text[: self.cursor_position]
                            + clipboard_text
                            + self.text[self.cursor_position :]
                        )
                        self.cursor_position += len(clipboard_text)

                    if len(new_text) <= self.max_length:
                        self.text = new_text
                    else:
                        self.text = new_text[: self.max_length]
                        self.cursor_position = min(
                            self.cursor_position, self.max_length
                        )
                        logging.warning(
                            "Pasted text exceeds max length and was truncated."
                        )
                    logging.info("Text pasted from clipboard.")

    def handle_mouse_input(
        self, mouse_x: int, mouse_y: int, is_mouse_button_down: bool
    ):
        """
        Handles mouse input for click-to-activate and drag-to-select functionality.
        This should be called continuously while the modal is active.
        """
        is_hovered_over_self = (
            mouse_x >= self.x
            and mouse_x <= (self.x + self.width)
            and mouse_y >= self.y
            and mouse_y <= (self.y + self.height)
        )

        if is_mouse_button_down:
            if (
                is_hovered_over_self
            ):  # If mouse button is down and hovered, we are either starting or continuing a drag
                if not self.is_active:  # If not active, activate it and start selection
                    self.set_active(True)
                    self.cursor_position = self.get_char_index_from_x(mouse_x)
                    self.selection_start = self.cursor_position
                    self.selection_end = self.cursor_position
                    self.is_dragging_selection = True
                elif self.is_active and self.is_dragging_selection:
                    # Continue dragging selection
                    self.selection_end = self.get_char_index_from_x(mouse_x)
                    self.cursor_position = self.selection_end  # Cursor follows drag end
                elif self.is_active and not self.is_dragging_selection:
                    # Clicked inside active field without dragging, this might be a new click to reposition cursor
                    self.cursor_position = self.get_char_index_from_x(mouse_x)
                    self.clear_selection()  # Clear any existing selection
                    self.is_dragging_selection = True  # Start new drag possibility
            else:  # Mouse button is down but not hovered over input field, so if dragging was active, stop it
                if self.is_dragging_selection:
                    self.is_dragging_selection = False
                    # If selection start and end are the same, clear selection (it was just a click)
                    if self.selection_start == self.selection_end:
                        self.clear_selection()
                    else:
                        # Normalize selection after drag
                        if (
                            self.selection_start is not None
                            and self.selection_end is not None
                        ):
                            if self.selection_start > self.selection_end:
                                self.selection_start, self.selection_end = (
                                    self.selection_end,
                                    self.selection_start,
                                )
        else:  # Mouse button is up
            if self.is_dragging_selection:
                # End of drag, normalize selection
                self.is_dragging_selection = False
                # If selection start and end are the same, clear selection (it was just a click)
                if self.selection_start == self.selection_end:
                    self.clear_selection()
                else:
                    # Ensure selection_start <= selection_end for consistent handling
                    if (
                        self.selection_start is not None
                        and self.selection_end is not None
                    ):
                        if self.selection_start > self.selection_end:
                            self.selection_start, self.selection_end = (
                                self.selection_end,
                                self.selection_start,
                            )

    def render(self):
        """Renders the input text box, selection highlight, and cursor."""
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

        max_text_width_for_display = self.width - 2 * text_padding_x

        # Determine the subset of text to render and its starting X position due to scrolling
        rendered_text = self.text
        text_width_px = self.object_layer_render.measure_text(
            rendered_text, self.font_size
        )

        self._text_offset_char_idx = 0  # Character index of the first visible character

        # Calculate the display_x based on scrolling logic
        # If text is longer than the input field, we need to adjust the starting X of the rendered text
        # such that the cursor position is always visible if possible.

        cursor_pixel_pos_in_full_text = self.object_layer_render.measure_text(
            self.text[: self.cursor_position], self.font_size
        )

        if text_width_px > max_text_width_for_display:
            # Calculate required offset to keep cursor in view
            # This logic centers the cursor if possible, or keeps it at the edge
            if cursor_pixel_pos_in_full_text > max_text_width_for_display:
                # If cursor is past the visible area, scroll to keep it in view
                # The _display_x will be negative, pushing text to the left
                # We want the cursor_pixel_pos_in_full_text to appear at the right edge
                # or within the field, e.g., max_text_width_for_display - cursor_pixel_pos_in_full_text

                # A more robust scroll: keep the cursor roughly in the center third,
                # but for simplicity, let's keep the end of the text visible or the cursor visible.

                # Calculate the start pixel for the portion of text that ends at the cursor
                text_up_to_cursor_width = self.object_layer_render.measure_text(
                    self.text[: self.cursor_position], self.font_size
                )

                if text_up_to_cursor_width > max_text_width_for_display:
                    # If text before cursor is longer than display area, calculate offset
                    self._display_x = (
                        self.x
                        + text_padding_x
                        - (text_up_to_cursor_width - max_text_width_for_display)
                    )
                else:
                    self._display_x = self.x + text_padding_x
            else:
                self._display_x = self.x + text_padding_x

            # This is a simplified way to determine rendered_text for visual scrolling.
            # A more precise way involves determining _text_offset_char_idx first, then slicing.
            # For now, this `_display_x` directly controls the drawing offset.

        else:
            self._display_x = self.x + text_padding_x

        display_y = (
            self.y + text_padding_y
        )  # Y position of text, accounts for vertical padding

        # Draw selection highlight
        if (
            self.selection_start is not None
            and self.selection_end is not None
            and self.selection_start != self.selection_end
        ):
            actual_start_idx = min(self.selection_start, self.selection_end)
            actual_end_idx = max(self.selection_start, self.selection_end)

            # Calculate pixel positions for the selection within the full text
            selection_start_pixel_full_text = self.object_layer_render.measure_text(
                self.text[:actual_start_idx], self.font_size
            )
            selection_end_pixel_full_text = self.object_layer_render.measure_text(
                self.text[:actual_end_idx], self.font_size
            )

            # Adjust selection pixels by the current _display_x offset
            draw_sel_x_start = self._display_x + selection_start_pixel_full_text
            draw_sel_x_end = self._display_x + selection_end_pixel_full_text

            # Clamp the drawing coordinates to the visible area of the input field
            clamped_draw_sel_x = max(self.x + text_padding_x, draw_sel_x_start)
            clamped_draw_sel_width = (
                min(draw_sel_x_end, self.x + self.width - text_padding_x)
                - clamped_draw_sel_x
            )

            if clamped_draw_sel_width > 0:
                self.object_layer_render.draw_rectangle(
                    int(clamped_draw_sel_x),
                    int(display_y),
                    int(clamped_draw_sel_width),
                    int(self.font_size),
                    self.selection_color,
                )

        # Draw text (with shading)
        self.object_layer_render.draw_text(
            rendered_text,
            self._display_x + 1,
            display_y + 1,
            self.font_size,
            self.shading_color,
        )
        self.object_layer_render.draw_text(
            rendered_text, self._display_x, display_y, self.font_size, self.text_color
        )

        # Draw cursor if active and visible (and no active selection unless dragging)
        if (
            self.is_active
            and self.cursor_visible
            and (
                self.selection_start == self.selection_end or self.is_dragging_selection
            )
        ):
            # Calculate cursor X position based on text before cursor
            cursor_x_offset_in_full_text = self.object_layer_render.measure_text(
                self.text[: self.cursor_position], self.font_size
            )

            cursor_x = self._display_x + cursor_x_offset_in_full_text

            # Clamp cursor drawing to the input field's visual bounds
            cursor_x = max(
                self.x + text_padding_x,
                min(cursor_x, self.x + self.width - text_padding_x),
            )

            cursor_y = display_y
            cursor_height = self.font_size
            cursor_width = 2
            self.object_layer_render.draw_rectangle(
                int(cursor_x),
                int(cursor_y),
                cursor_width,
                cursor_height,
                self.text_color,
            )

    def check_click(
        self, mouse_x: int, mouse_y: int, is_mouse_button_pressed: bool
    ) -> bool:
        """
        Checks if the input field was clicked.
        This method primarily handles the initial click to activate the field and start a potential drag.
        Returns True if clicked, False otherwise.
        """
        is_hovered = (
            mouse_x >= self.x
            and mouse_x <= (self.x + self.width)
            and mouse_y >= self.y
            and mouse_y <= (self.y + self.height)
        )

        if is_hovered and is_mouse_button_pressed:
            if not self.is_active:  # If not active, activate it
                self.set_active(True)

            # Always set cursor position and start selection on a new click within the field
            self.cursor_position = self.get_char_index_from_x(mouse_x)
            self.selection_start = self.cursor_position
            self.selection_end = self.cursor_position
            self.is_dragging_selection = True  # Assume drag might start

            logging.debug(
                f"Input field clicked. Active: {self.is_active}, Cursor pos: {self.cursor_position}, Sel: ({self.selection_start},{self.selection_end})"
            )
            return True
        elif is_mouse_button_pressed and self.is_active and not is_hovered:
            # Clicked outside while active, deactivate and clear selection
            self.set_active(False)
            logging.debug(f"Input field deactivated. Active: {self.is_active}")
            return False

        return False

    def get_text(self) -> str:
        """Returns the current text in the input field."""
        return self.text

    def set_text(self, new_text: str):
        """Sets the text of the input field, clamping to max_length."""
        self.text = new_text[: self.max_length]
        self.cursor_position = len(self.text)  # Move cursor to end of new text
        self.clear_selection()

    def is_currently_active(self) -> bool:
        """Returns true if the input field is currently active."""
        return self.is_active
