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
    begin_scissor_mode,  # Import for clipping
    end_scissor_mode,  # Import for clipping
)

logging.basicConfig(level=logging.INFO, format="%s - %(levelname)s - %(message)s")


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

        # Internal rendering properties for text scrolling
        # Define separate padding for left and right as requested
        self.text_padding_left = 10
        self.text_padding_right = 10

        # Index of the first character visible in the input field.
        # This will be updated by _update_text_offset_for_scroll and used for drawing
        self._text_offset_char_idx: int = 0
        # Actual X position where text drawing starts, accounting for left padding
        self._display_x: int = self.x + self.text_padding_left

    def set_active(self, active: bool):
        """Sets the active state of the input field."""
        self.is_active = active
        self.cursor_blink_timer = 0.0  # Reset timer when activation changes
        self.cursor_visible = True  # Ensure cursor is visible when activated
        if not active:
            self.clear_selection()
        # Adjust scroll when activation changes, ensuring the cursor is in view
        # Use adjusted width for text display
        self._update_text_offset_for_scroll(
            self.width - (self.text_padding_left + self.text_padding_right)
        )

    def clear_selection(self):
        """Clears any active text selection."""
        self.selection_start = None
        self.selection_end = None
        self.is_dragging_selection = False

    def get_char_index_from_x(self, mouse_x: int) -> int:
        """
        Calculates the character index in the full text string closest to a given mouse X coordinate
        within the input field's text area, considering the current scroll offset.
        """
        # The x-coordinate relative to the start of the visible text drawing area (self._display_x).
        relative_x_to_display_start = mouse_x - self._display_x

        # If click is before the visible text (left padding area or scrolled off)
        if relative_x_to_display_start < 0:
            return self._text_offset_char_idx

        # Calculate the pixel width of the text segment *before* the current offset.
        # This helps in mapping the `relative_x_to_display_start` to an absolute pixel position
        # within the full text string.
        pixel_width_before_offset = self.object_layer_render.measure_text(
            self.text[: self._text_offset_char_idx], self.font_size
        )
        # Effective mouse X position relative to the absolute beginning of the full text string.
        effective_mouse_x_relative_to_full_text_start = (
            relative_x_to_display_start + pixel_width_before_offset
        )

        cumulative_pixel_width = 0
        # Iterate through the *entire* string from index 0 to find the character.
        for i in range(len(self.text)):
            char_width = self.object_layer_render.measure_text(
                self.text[i], self.font_size
            )

            # If the effective mouse X is within this character's bounds (midpoint logic)
            if (
                effective_mouse_x_relative_to_full_text_start
                <= cumulative_pixel_width + (char_width / 2)
            ):
                return i
            cumulative_pixel_width += char_width

        # If the loop finishes, it means the mouse click was past the end of the text.
        return len(self.text)

    def _update_text_offset_for_scroll(self, max_text_width_for_display: int):
        """
        Adjusts _text_offset_char_idx to ensure the cursor is always visible within the input field.
        The goal is to scroll the text horizontally so that the cursor remains within the
        visible bounds of the input field.
        """
        if not self.text:
            self._text_offset_char_idx = 0
            return

        # Calculate the pixel width of the text from the current offset up to the cursor position.
        cursor_pixel_from_current_offset = self.object_layer_render.measure_text(
            self.text[self._text_offset_char_idx : self.cursor_position], self.font_size
        )

        # If cursor is past the right edge of the visible window (from current offset)
        if cursor_pixel_from_current_offset > max_text_width_for_display:
            # Shift _text_offset_char_idx right until cursor is visible at the right edge.
            # We want to find the furthest left character (new_offset_idx) such that the
            # text from new_offset_idx to cursor_position fits within max_text_width_for_display.
            new_offset_idx = self.cursor_position
            temp_width = 0
            while new_offset_idx > 0:
                char_width = self.object_layer_render.measure_text(
                    self.text[new_offset_idx - 1], self.font_size
                )
                if temp_width + char_width > max_text_width_for_display:
                    break  # This character would make the segment too wide
                temp_width += char_width
                new_offset_idx -= 1
            self._text_offset_char_idx = new_offset_idx

        # If cursor is past the left edge of the visible window (its absolute index is smaller than the offset)
        elif self.cursor_position < self._text_offset_char_idx:
            # Shift _text_offset_char_idx to the cursor position.
            # This ensures the cursor becomes the first visible character on the left.
            self._text_offset_char_idx = self.cursor_position

        # Ensure the text is always left-aligned if it fits entirely.
        total_text_pixel_width = self.object_layer_render.measure_text(
            self.text, self.font_size
        )
        if total_text_pixel_width <= max_text_width_for_display:
            self._text_offset_char_idx = 0
        else:
            # If the text is longer than the display, ensure that the offset is such
            # that the end of the text is shown if the cursor is at the end,
            # or if the text is pulled right.
            # This is critical to prevent unwanted blank space at the right when scrolling text from the left.

            # Calculate the ideal offset if the text were right-aligned.
            ideal_end_offset_idx = len(self.text)
            temp_width_from_end = 0
            while ideal_end_offset_idx > 0:
                char_width = self.object_layer_render.measure_text(
                    self.text[ideal_end_offset_idx - 1], self.font_size
                )
                if temp_width_from_end + char_width > max_text_width_for_display:
                    break
                temp_width_from_end += char_width
                ideal_end_offset_idx -= 1

            # If the cursor is at the very end, force the scroll to show the end.
            if self.cursor_position == len(self.text):
                self._text_offset_char_idx = ideal_end_offset_idx
            else:
                # If the current offset is too far left (causing blank space on right)
                # and the cursor is already in view, try to shift it right to fill the space.
                current_visible_segment_width = self.object_layer_render.measure_text(
                    self.text[self._text_offset_char_idx :], self.font_size
                )
                if current_visible_segment_width < max_text_width_for_display:
                    # More precisely, ensure the current offset is not less than the ideal end offset
                    # if the current segment from offset is too short.
                    self._text_offset_char_idx = max(
                        self._text_offset_char_idx, ideal_end_offset_idx
                    )

        # Final clamping to ensure _text_offset_char_idx is within valid bounds
        self._text_offset_char_idx = max(
            0, min(self._text_offset_char_idx, len(self.text))
        )

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
            # Update scroll after text change
            self._update_text_offset_for_scroll(
                self.width - (self.text_padding_left + self.text_padding_right)
            )

        # Handle special keys
        if (
            key_pressed
        ):  # key_pressed will now contain repeating keys from KeyboardCoreComponent
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
                # Update scroll after text change
                self._update_text_offset_for_scroll(
                    self.width - (self.text_padding_left + self.text_padding_right)
                )

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
                # Update scroll after text change
                self._update_text_offset_for_scroll(
                    self.width - (self.text_padding_left + self.text_padding_right)
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
                # Update scroll after cursor move
                self._update_text_offset_for_scroll(
                    self.width - (self.text_padding_left + self.text_padding_right)
                )

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
                # Update scroll after cursor move
                self._update_text_offset_for_scroll(
                    self.width - (self.text_padding_left + self.text_padding_right)
                )

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
                # Update scroll after cursor move
                self._update_text_offset_for_scroll(
                    self.width - (self.text_padding_left + self.text_padding_right)
                )

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
                # Update scroll after cursor move
                self._update_text_offset_for_scroll(
                    self.width - (self.text_padding_left + self.text_padding_right)
                )

            elif key_pressed == KEY_A and is_control_down:  # Ctrl+A (Select All)
                self.selection_start = 0
                self.selection_end = len(self.text)
                self.cursor_position = len(
                    self.text
                )  # Move cursor to end of selected text
                logging.info("Select All triggered.")
                # Update scroll after selection change
                self._update_text_offset_for_scroll(
                    self.width - (self.text_padding_left + self.text_padding_right)
                )

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
                # No scroll update needed for copy

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
                # Update scroll after text change
                self._update_text_offset_for_scroll(
                    self.width - (self.text_padding_left + self.text_padding_right)
                )

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
                # Update scroll after text change
                self._update_text_offset_for_scroll(
                    self.width - (self.text_padding_left + self.text_padding_right)
                )

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
                    # Cursor and selection start will be set in check_click
                    self.cursor_position = self.get_char_index_from_x(mouse_x)
                    self.selection_start = self.cursor_position
                    self.selection_end = self.cursor_position
                    self.is_dragging_selection = True
                    # Update scroll after click
                    self._update_text_offset_for_scroll(
                        self.width - (self.text_padding_left + self.text_padding_right)
                    )
                elif self.is_active and self.is_dragging_selection:
                    # Continue dragging selection
                    self.selection_end = self.get_char_index_from_x(mouse_x)
                    self.cursor_position = self.selection_end  # Cursor follows drag end
                    # Update scroll during drag
                    self._update_text_offset_for_scroll(
                        self.width - (self.text_padding_left + self.text_padding_right)
                    )
                elif self.is_active and not self.is_dragging_selection:
                    # Clicked inside active field without dragging, this might be a new click to reposition cursor
                    self.cursor_position = self.get_char_index_from_x(mouse_x)
                    self.clear_selection()  # Clear any existing selection
                    self.is_dragging_selection = True  # Start new drag possibility
                    # Update scroll after click
                    self._update_text_offset_for_scroll(
                        self.width - (self.text_padding_left + self.text_padding_right)
                    )
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
                    # No scroll update needed at end of drag unless text changes

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
                # No scroll update needed at end of drag unless text changes

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

        # The maximum pixel width the text can occupy, considering both left and right padding
        max_text_width_for_display = self.width - (
            self.text_padding_left + self.text_padding_right
        )

        # Update scroll offset before rendering
        self._update_text_offset_for_scroll(max_text_width_for_display)

        # The actual X position where the *first character of the visible text* will be drawn, accounting for left padding
        self._display_x = self.x + self.text_padding_left

        # Calculate vertical position for text, centered
        display_y = self.y + (self.height - self.font_size) // 2

        # Define the clipping rectangle for text and cursor
        # It's important to use integers for the clipping rectangle coordinates.
        clipping_rect_x = int(self.x + self.text_padding_left)
        clipping_rect_y = int(self.y)
        clipping_rect_width = int(
            self.width - self.text_padding_left - self.text_padding_right
        )
        clipping_rect_height = int(self.height)

        # Begin clipping mode
        begin_scissor_mode(
            clipping_rect_x, clipping_rect_y, clipping_rect_width, clipping_rect_height
        )

        # Get the visible portion of the text based on the calculated offset and clip it
        full_text_from_offset = self.text[self._text_offset_char_idx :]

        text_to_draw = ""
        current_pixel_width = 0
        for char in full_text_from_offset:
            char_width = self.object_layer_render.measure_text(char, self.font_size)
            # Add a small buffer (e.g., 1 pixel) to account for potential drawing discrepancies
            # This helps prevent characters from being clipped prematurely.
            if current_pixel_width + char_width <= max_text_width_for_display + 1:
                text_to_draw += char
                current_pixel_width += char_width
            else:
                break  # Stop adding characters if they exceed the visible width

        # Draw selection highlight
        if (
            self.selection_start is not None
            and self.selection_end is not None
            and self.selection_start != self.selection_end
        ):
            actual_start_idx = min(self.selection_start, self.selection_end)
            actual_end_idx = max(self.selection_start, self.selection_end)

            # Clamp the selection indices to the currently visible text range
            visible_sel_start = max(actual_start_idx, self._text_offset_char_idx)
            visible_sel_end = min(
                actual_end_idx, len(self.text)
            )  # Ensure end is not past actual text length

            if (
                visible_sel_start < visible_sel_end
            ):  # Only draw if there's a visible selection segment
                # Calculate pixel positions for the selection within the *visible* text segment
                selection_start_pixel_relative_to_offset = (
                    self.object_layer_render.measure_text(
                        self.text[self._text_offset_char_idx : visible_sel_start],
                        self.font_size,
                    )
                )
                selection_end_pixel_relative_to_offset = (
                    self.object_layer_render.measure_text(
                        self.text[self._text_offset_char_idx : visible_sel_end],
                        self.font_size,
                    )
                )

                draw_sel_x_start = (
                    self._display_x + selection_start_pixel_relative_to_offset
                )
                draw_sel_x_end = (
                    self._display_x + selection_end_pixel_relative_to_offset
                )

                # Clamp selection drawing to the input field's visual bounds
                clamped_draw_sel_x = max(self._display_x, int(draw_sel_x_start))
                clamped_draw_sel_width = (
                    min(
                        int(draw_sel_x_end),
                        self._display_x + max_text_width_for_display,
                    )
                    - clamped_draw_sel_x
                )

                if clamped_draw_sel_width > 0:
                    self.object_layer_render.draw_rectangle(
                        clamped_draw_sel_x,
                        int(display_y),
                        clamped_draw_sel_width,
                        int(self.font_size),
                        self.selection_color,
                    )

        # Draw text (with shading)
        self.object_layer_render.draw_text(
            text_to_draw,
            self._display_x + 1,
            display_y + 1,
            self.font_size,
            self.shading_color,
        )
        self.object_layer_render.draw_text(
            text_to_draw, self._display_x, display_y, self.font_size, self.text_color
        )

        # Draw cursor if active and visible (and no active selection unless dragging)
        if (
            self.is_active
            and self.cursor_visible
            and (
                self.selection_start == self.selection_end or self.is_dragging_selection
            )
        ):
            # Calculate cursor X position relative to the *start of the drawn text*
            cursor_x_offset_relative_to_drawn_text = (
                self.object_layer_render.measure_text(
                    self.text[self._text_offset_char_idx : self.cursor_position],
                    self.font_size,
                )
            )

            cursor_x = self._display_x + cursor_x_offset_relative_to_drawn_text

            # Clamp cursor drawing to the input field's visual bounds (self._display_x to self._display_x + max_text_width_for_display)
            cursor_x = max(
                self._display_x,
                min(cursor_x, self._display_x + max_text_width_for_display),
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

        # End clipping mode
        end_scissor_mode()

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
                f"Input field clicked. Active: %s, Cursor pos: %s, Sel: (%s,%s)",
                self.is_active,
                self.cursor_position,
                self.selection_start,
                self.selection_end,
            )
            self._update_text_offset_for_scroll(
                self.width - (self.text_padding_left + self.text_padding_right)
            )
            return True
        elif is_mouse_button_pressed and self.is_active and not is_hovered:
            # Clicked outside while active, deactivate and clear selection
            self.set_active(False)
            logging.debug("Input field deactivated. Active: %s", self.is_active)
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
        # Update scroll after setting text
        self._update_text_offset_for_scroll(
            self.width - (self.text_padding_left + self.text_padding_right)
        )

    def is_currently_active(self) -> bool:
        """Returns true if the input field is currently active."""
        return self.is_active
