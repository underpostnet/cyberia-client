import logging
from raylibpy import (
    Color,
    KEY_ENTER,
    Vector2,
    Rectangle,
    get_mouse_wheel_move,
    check_collision_point_rec,
    begin_scissor_mode,
    end_scissor_mode,
    is_mouse_button_down,
    is_mouse_button_pressed,
    MOUSE_LEFT_BUTTON,
)
from config import (
    UI_FONT_SIZE,
    UI_TEXT_COLOR_PRIMARY,
    UI_TEXT_COLOR_SHADING,
    KEYBOARD_BACKSPACE_INITIAL_DELAY,
    KEYBOARD_BACKSPACE_REPEAT_RATE,
    CHAT_HISTORY_PADDING_X,
    CHAT_HISTORY_PADDING_Y_TOP,
    CHAT_HISTORY_PADDING_Y_BOTTOM,
    CHAT_MESSAGE_LINE_SPACING,
    CHAT_MESSAGE_PADDING_Y,
    SCROLLBAR_WIDTH,
    SCROLLBAR_PADDING,
    SCROLLBAR_TRACK_COLOR_TUPLE,
    SCROLLBAR_THUMB_COLOR_TUPLE,
    CHAT_WHEEL_SCROLL_SENSITIVITY,
)
from ui.components.core.grid_core_component import GridCoreComponent
from ui.components.core.input_text_core_component import (
    InputTextCoreComponent,
)  # New import
from object_layer.object_layer_render import ObjectLayerRender
from network_state.network_state_proxy import NetworkStateProxy  # For sending messages
from ui.components.core.scrollbar_core_component import ScrollbarCoreComponent

logging.basicConfig(
    level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s"
)

# Example chat room data structure
CHAT_ROOM_DATA = [
    {
        "id": "chat_001",
        "name": "General Chat",
        "description": "A public channel for general discussions.",
        "members": 150,
        "last_message": "Hello everyone!",
        "messages": [
            {"sender": "Alice", "text": "Hello everyone!", "time": "10:00"},
            {"sender": "Bob", "text": "Hi Alice!", "time": "10:01"},
            {"sender": "Charlie", "text": "What's up?", "time": "10:05"},
        ],
    },
    {
        "id": "chat_002",
        "name": "Trade Hub",
        "description": "Buy, sell, and trade items with other players.",
        "members": 75,
        "last_message": "Looking for rare components.",
        "messages": [
            {"sender": "TraderX", "text": "Selling rare components!", "time": "09:30"},
            {"sender": "BuyerY", "text": "How much?", "time": "09:35"},
            {"sender": "TraderX", "text": "DM me for details.", "time": "09:40"},
        ],
    },
    {
        "id": "chat_003",
        "name": "Quest Talk",
        "description": "Discuss quests, share tips, and find groups.",
        "members": 90,
        "last_message": "Need help with 'The Missing Spark'.",
        "messages": [
            {
                "sender": "Questor1",
                "text": "Stuck on 'The Missing Spark'.",
                "time": "11:00",
            },
            {
                "sender": "HelperBot",
                "text": "Check Sector 4, abandoned factory.",
                "time": "11:02",
            },
            {"sender": "Questor1", "text": "Thanks!", "time": "11:05"},
        ],
    },
    {
        "id": "chat_004",
        "name": "Dev Lounge",
        "description": "A private channel for developers.",
        "members": 5,
        "last_message": "New feature deployed.",
        "messages": [
            {"sender": "DevLead", "text": "New feature deployed.", "time": "14:00"},
            {"sender": "Dev2", "text": "Looks good!", "time": "14:05"},
        ],
    },
]


class ChatCyberiaView:
    """
    Manages the display and interaction for the chat interface.
    Uses GridCoreComponent to render chat rooms in a list bar format.
    """

    def __init__(
        self,
        object_layer_render_instance: ObjectLayerRender,
        network_proxy: NetworkStateProxy,
    ):
        self.object_layer_render = object_layer_render_instance
        self.network_proxy = network_proxy
        self.title_text = "Chat Rooms"
        self.chat_rooms = CHAT_ROOM_DATA
        self.message_input_placeholder = "Type your message..."  # Store placeholder
        self.selected_chat_index: int | None = None  # To track selected chat room

        # Configure GridCoreComponent for a list bar form (1 column, multiple rows)
        self.grid_component = GridCoreComponent(
            object_layer_render_instance=self.object_layer_render,
            num_rows=len(
                self.chat_rooms
            ),  # Number of rows based on number of chat rooms
            num_cols=1,  # Single column for list bar
            item_width=280,  # Width of each chat room item
            item_height=80,  # Height of each chat room item
            item_padding=5,  # Padding between chat room items
            render_item_callback=self._render_chat_room_item,
            background_color=Color(
                0, 0, 0, 0
            ),  # Transparent background, main modal draws it
            border_color=Color(50, 50, 50, 200),
            slot_background_color=Color(30, 30, 30, 180),
            slot_hover_color=Color(50, 50, 50, 200),
            slot_selected_color=Color(
                150, 150, 0, 200
            ),  # More prominent selected color
            grid_type="rectangle",  # Explicitly set to rectangle
        )

        # Initialize InputTextCoreComponent for message input
        self.message_input = InputTextCoreComponent(
            object_layer_render_instance=self.object_layer_render,
            x=0,  # Will be set dynamically in render_single_chat_detail
            y=0,  # Will be set dynamically
            width=0,  # Will be set dynamically
            height=40,
            font_size=UI_FONT_SIZE,
            text_color=Color(*UI_TEXT_COLOR_PRIMARY),
            background_color=Color(50, 50, 50, 200),
            border_color=Color(100, 100, 100, 255),
            shading_color=Color(*UI_TEXT_COLOR_SHADING),
            initial_text=self.message_input_placeholder,  # Use stored placeholder
        )

        # Scrollbar and history display area (initialized with dummy values, set in _render_single_chat_detail)
        self.history_display_rect = Rectangle(0, 0, 0, 0)
        self.scrollbar = ScrollbarCoreComponent(
            x=0,
            y=0,
            width=SCROLLBAR_WIDTH,
            height=0,  # Positioned dynamically
            track_color=Color(*SCROLLBAR_TRACK_COLOR_TUPLE),
            thumb_color=Color(*SCROLLBAR_THUMB_COLOR_TUPLE),
        )
        self._history_content_height = 0.0
        self._scroll_offset_y = 0.0
        self._last_rendered_messages_count = 0  # To detect changes in messages

        # Store message font size and calculated line height for messages
        self.message_font_size = UI_FONT_SIZE - 4
        self.message_line_render_height = (
            self.message_font_size + CHAT_MESSAGE_LINE_SPACING
        )

    def _render_chat_room_item(
        self,
        object_layer_render_instance: ObjectLayerRender,
        x: int,
        y: int,
        width: int,
        height: int,
        chat_data: dict,
        is_hovered: bool,
        is_selected: bool,
    ):
        """
        Renders the content of a single chat room item within the grid slot.
        This is a callback passed to GridCoreComponent.
        """
        text_color = Color(*UI_TEXT_COLOR_PRIMARY)
        shading_color = Color(*UI_TEXT_COLOR_SHADING)

        name = chat_data.get("name", "No Name")
        description = chat_data.get("description", "No description available.")
        members = chat_data.get("members", 0)
        last_message = chat_data.get("last_message", "No recent messages.")

        # Draw Name
        object_layer_render_instance.draw_text(
            name, x + 10 + 1, y + 10 + 1, UI_FONT_SIZE, shading_color
        )
        object_layer_render_instance.draw_text(
            name, x + 10, y + 10, UI_FONT_SIZE, text_color
        )

        # Draw Members
        members_text = f"Members: {members}"
        object_layer_render_instance.draw_text(
            members_text, x + 10 + 1, y + 30 + 1, UI_FONT_SIZE - 2, shading_color
        )
        object_layer_render_instance.draw_text(
            members_text, x + 10, y + 30, UI_FONT_SIZE - 2, text_color
        )

        # Draw Last Message
        last_message_text = f"Last: {last_message}"
        object_layer_render_instance.draw_text(
            last_message_text, x + 10 + 1, y + 50 + 1, UI_FONT_SIZE - 2, shading_color
        )
        object_layer_render_instance.draw_text(
            last_message_text, x + 10, y + 50, UI_FONT_SIZE - 2, text_color
        )

        # Optionally draw a visual indicator for selected item
        if is_selected:
            self.object_layer_render.draw_rectangle(
                x + width - 10,
                y + 10,
                5,
                5,
                Color(255, 255, 0, 255),  # Small yellow square
            )

    def _update_history_dimensions_and_scrollbar(self, messages: list):
        """
        Calculates the total height of chat messages and updates the scrollbar.
        This should be called when messages change or the view/history_display_rect resizes.
        """
        if not messages:
            self._history_content_height = 0.0
        else:
            total_height = 0.0
            for i, msg in enumerate(messages):
                sender_line_height = (
                    self.message_line_render_height
                )  # Height for sender + time

                wrapped_text_lines = self._wrap_text(
                    msg.get("text", ""),
                    self.history_display_rect.width,  # Use actual display width for wrapping
                    self.message_font_size,
                )
                if not wrapped_text_lines:
                    wrapped_text_lines = [""]  # Ensure at least one line for structure

                text_block_height = (
                    len(wrapped_text_lines) * self.message_line_render_height
                )
                total_height += sender_line_height + text_block_height

                if i < len(messages) - 1:
                    total_height += CHAT_MESSAGE_PADDING_Y  # Padding between messages

            self._history_content_height = total_height

        self.scrollbar.set_content_dimensions(
            self._history_content_height, self.history_display_rect.height
        )

        # If content height changed, current scroll_offset_y might be invalid, re-fetch
        self._scroll_offset_y = self.scrollbar.get_scroll_offset()

        # If new messages were added, and we were at the bottom, stay at the bottom
        # This logic might need refinement based on desired auto-scroll behavior

    def _render_single_chat_detail(
        self,
        modal_component,
        x: int,
        y: int,
        width: int,
        height: int,
        chat_data: dict,
        mouse_x: int,
        mouse_y: int,
        is_mouse_button_pressed_left: bool,  # Specifically for left button
        is_mouse_button_down: bool,  # New: is_mouse_button_down
        char_pressed: int | None,
        key_pressed: int | None,
        is_key_down_map: dict,
        dt: float,
    ):
        """
        Renders the detailed view of a single selected chat room.
        """
        text_color = Color(*UI_TEXT_COLOR_PRIMARY)
        # Ensure UI_TEXT_COLOR_SHADING is a tuple of 4 (RGBA)
        shading_color_tuple = (
            UI_TEXT_COLOR_SHADING
            if len(UI_TEXT_COLOR_SHADING) == 4
            else (*UI_TEXT_COLOR_SHADING, 255)
        )
        shading_color = Color(*shading_color_tuple)
        shading_color = Color(*UI_TEXT_COLOR_SHADING)

        name = chat_data.get("name", "No Name")
        description = chat_data.get("description", "No description available.")
        messages = chat_data.get("messages", [])

        self.title_text = name  # Update ChatCyberiaView's internal title
        modal_component.set_title(self.title_text)  # Set the modal's title

        # Draw Chat Room Name
        name_width = self.object_layer_render.measure_text(name, UI_FONT_SIZE + 5)
        self.object_layer_render.draw_text(
            name,
            x + (width - name_width) // 2 + 1,
            y + 20 + 1,
            UI_FONT_SIZE + 5,
            shading_color,
        )
        self.object_layer_render.draw_text(
            name, x + (width - name_width) // 2, y + 20, UI_FONT_SIZE + 5, text_color
        )

        # Draw Description
        desc_start_y = y + 60  # Adjusted for tighter layout
        max_desc_width = width - 40
        desc_lines = self._wrap_text(description, max_desc_width, UI_FONT_SIZE - 2)

        current_y = desc_start_y
        for line in desc_lines:
            self.object_layer_render.draw_text(
                line, x + 20 + 1, current_y + 1, UI_FONT_SIZE - 2, shading_color
            )
            self.object_layer_render.draw_text(
                line, x + 20, current_y, UI_FONT_SIZE - 2, text_color
            )
            current_y += (UI_FONT_SIZE - 2) + 5
        current_y += CHAT_HISTORY_PADDING_Y_TOP  # Padding before messages start

        # Define history display rectangle for messages
        self.history_display_rect.x = x + CHAT_HISTORY_PADDING_X
        self.history_display_rect.y = current_y
        self.history_display_rect.width = (
            width - (2 * CHAT_HISTORY_PADDING_X) - SCROLLBAR_WIDTH - SCROLLBAR_PADDING
        )

        input_box_total_height = (
            self.message_input.height + CHAT_HISTORY_PADDING_Y_BOTTOM
        )
        self.history_display_rect.height = (
            y + height - input_box_total_height
        ) - self.history_display_rect.y

        # Update scrollbar geometry
        self.scrollbar.rect.x = (
            self.history_display_rect.x
            + self.history_display_rect.width
            + SCROLLBAR_PADDING
        )
        self.scrollbar.rect.y = self.history_display_rect.y
        self.scrollbar.rect.height = self.history_display_rect.height

        # Update scrollbar content dimensions if messages changed
        if (
            len(messages) != self._last_rendered_messages_count
            or self.scrollbar.visible_height != self.history_display_rect.height
        ):
            self._update_history_dimensions_and_scrollbar(messages)
            self._last_rendered_messages_count = len(messages)

        # Handle scrollbar events
        mouse_pos = Vector2(float(mouse_x), float(mouse_y))
        mouse_wheel = get_mouse_wheel_move()

        # Pass is_mouse_button_down for the *specific* button the scrollbar cares about (left)
        # is_mouse_button_down is a general param, might be for any button if not specified
        # For this component, we'll assume it's for the left button if true.
        # More robustly, pass rl.is_mouse_button_down(rl.MOUSE_LEFT_BUTTON) and rl.is_mouse_button_pressed(rl.MOUSE_LEFT_BUTTON)
        # The current `is_mouse_button_down` param from modal is likely general.
        # Let's assume `is_mouse_button_down` means left button is down for simplicity here.
        # `is_mouse_button_pressed` is fine as it's a per-frame press.

        scrollbar_consumed_event = self.scrollbar.handle_event(
            mouse_pos,
            is_mouse_button_down,  # Assuming this implies left button for dragging
            is_mouse_button_pressed_left,  # This is specific to left button press
            mouse_wheel,
            CHAT_WHEEL_SCROLL_SENSITIVITY,
        )
        if scrollbar_consumed_event:
            self._scroll_offset_y = self.scrollbar.get_scroll_offset()
        # If scrollbar didn't consume wheel, but mouse is over history_display_rect, handle it
        elif (
            check_collision_point_rec(mouse_pos, self.history_display_rect)
            and mouse_wheel != 0.0
        ):
            if self.scrollbar.content_height > self.scrollbar.visible_height:
                self.scrollbar.set_scroll_position(
                    self.scrollbar.scroll_position
                    - (mouse_wheel * CHAT_WHEEL_SCROLL_SENSITIVITY)
                )
                self._scroll_offset_y = self.scrollbar.get_scroll_offset()

        # Render Messages within Scissor Rectangle
        begin_scissor_mode(
            int(self.history_display_rect.x),
            int(self.history_display_rect.y),
            int(self.history_display_rect.width),
            int(self.history_display_rect.height),
        )

        current_render_y = self.history_display_rect.y - self._scroll_offset_y
        for msg in messages:
            sender_line_text = f"[{msg.get('time')}] {msg.get('sender')}:"  # Space after colon removed previously
            wrapped_text_lines = self._wrap_text(
                msg.get("text", ""),
                self.history_display_rect.width,
                self.message_font_size,
            )
            if not wrapped_text_lines:
                wrapped_text_lines = [""]

            # Calculate message block height for visibility check
            msg_block_height = (
                1 + len(wrapped_text_lines)
            ) * self.message_line_render_height + CHAT_MESSAGE_PADDING_Y

            # Basic culling: only draw if part of the message block is visible
            if (
                current_render_y + msg_block_height > self.history_display_rect.y
                and current_render_y
                < self.history_display_rect.y + self.history_display_rect.height
            ):

                # Draw sender line
                self.object_layer_render.draw_text(
                    sender_line_text,
                    x + 20 + 1,
                    int(current_render_y + 1),
                    self.message_font_size,
                    shading_color,
                )
                self.object_layer_render.draw_text(
                    sender_line_text,
                    x + 20,
                    int(current_render_y),
                    self.message_font_size,
                    text_color,
                )
                current_render_y += self.message_line_render_height

                # Draw wrapped message text lines
                message_body_start_x = x + 20  # Aligned with sender line
                for line_text in wrapped_text_lines:
                    self.object_layer_render.draw_text(
                        line_text,
                        message_body_start_x + 1,
                        int(current_render_y + 1),
                        self.message_font_size,
                        shading_color,
                    )
                    self.object_layer_render.draw_text(
                        line_text,
                        message_body_start_x,
                        int(current_render_y),
                        self.message_font_size,
                        text_color,
                    )
                    current_render_y += self.message_line_render_height

                current_render_y += (
                    CHAT_MESSAGE_PADDING_Y  # Padding after each message block
                )
            else:
                # If message block is not visible, just advance the render position
                current_render_y += msg_block_height  # Skip rendering this block

        end_scissor_mode()
        self.scrollbar.render()  # Render scrollbar on top of messages, but within modal

        # Position and render message input field
        input_box_y = (
            y
            + height
            - self.message_input.height
            - (CHAT_HISTORY_PADDING_Y_BOTTOM // 2)  # Centered in bottom padding
        )
        input_box_x = x + 20
        input_box_width = width - 40

        self.message_input.x = input_box_x
        self.message_input.y = input_box_y
        self.message_input.width = input_box_width

        # Only pass mouse to input if not consumed by scrollbar for the same press
        # This simple check might not be enough if scrollbar consumes hover without press
        # A more robust system would involve an event consumption flag.
        if not (scrollbar_consumed_event and is_mouse_button_pressed_left):
            self.message_input.handle_mouse_input(
                mouse_x, mouse_y, is_mouse_button_down
            )

        # Update input text component with keyboard events and delta time
        self.message_input.update(dt, char_pressed, key_pressed, is_key_down_map)
        self.message_input.render()

        # Handle sending message on Enter key press
        if (
            key_pressed == KEY_ENTER
            and self.message_input.is_active
            and self.message_input.text.strip() != ""
            and self.message_input.text
            != self.message_input_placeholder  # Compare with stored placeholder
        ):
            message_text = self.message_input.text
            current_room_id = chat_data.get("id")
            if current_room_id and self.network_proxy:
                logging.info(
                    f"Sending chat message: '{message_text}' to room: {current_room_id}"
                )
                self.network_proxy.send_client_message(
                    "client_chat_message",
                    {"room_id": current_room_id, "text": message_text},
                )
                self.message_input.set_text("")  # Clear input after sending
            else:
                logging.warning(
                    "Cannot send chat message: No room selected or network proxy unavailable."
                )

    def _wrap_text(self, text: str, max_width: int, font_size: int) -> list[str]:
        """Wraps text to fit within a maximum width."""
        words = text.split(" ")
        lines = []
        current_line = []
        for word in words:
            # Check if current_line is empty to avoid leading space on new lines
            test_line_text = " ".join(current_line + [word]) if current_line else word
            if (
                self.object_layer_render.measure_text(test_line_text, font_size)
                <= max_width
            ):
                current_line.append(word)
            else:
                # If current_line is not empty, add it to lines
                if current_line:
                    lines.append(" ".join(current_line))
                # Start new line with the current word, but only if it fits by itself
                # If a single word is too long, it will overflow.
                # A more robust solution would break long words.
                if self.object_layer_render.measure_text(word, font_size) <= max_width:
                    current_line = [word]
                else:  # Word itself is too long, add it and let it overflow (or implement word breaking)
                    lines.append(word)
                    current_line = []
        if current_line:
            lines.append(" ".join(current_line))

        # If the original text was empty or only spaces and resulted in no lines, return a list with one empty string.
        if not lines and not text.strip():
            return [""]
        return lines

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
        Renders the chat room list view or a single chat detail view.
        Args:
            x, y, width, height: Dimensions of the modal container.
            mouse_x, mouse_y: Current mouse coordinates.
            is_mouse_button_pressed: True if the primary mouse button is pressed in the current frame.
            modal_component: The modal component instance (contains data_to_pass including keyboard data)
        """
        # Extract keyboard data, mouse button down state, and delta time from data_to_pass
        data_to_pass = modal_component.data_to_pass
        char_pressed = data_to_pass.get("char_pressed")
        key_pressed = data_to_pass.get("key_pressed")
        is_key_down_map = data_to_pass.get("is_key_down_map")
        dt = data_to_pass.get("dt")
        is_mouse_button_down = data_to_pass.get(
            "is_mouse_button_down", False
        )  # General mouse button down state

        if self.selected_chat_index is None:
            # Render the chat room list
            self.title_text = "Chat Rooms"  # Reset internal title for list view
            modal_component.set_title(self.title_text)  # Set the modal's title

            # Draw the title for the grid
            title_text = self.title_text
            title_font_size = UI_FONT_SIZE + 2
            title_text_width = self.object_layer_render.measure_text(
                title_text, title_font_size
            )
            title_x = x + (width - title_text_width) // 2
            title_y = y + 20

            self.object_layer_render.draw_text(
                title_text,
                title_x + 1,
                title_y + 1,
                title_font_size,
                Color(*UI_TEXT_COLOR_SHADING),
            )
            self.object_layer_render.draw_text(
                title_text,
                title_x,
                title_y,
                title_font_size,
                Color(*UI_TEXT_COLOR_PRIMARY),
            )

            # Adjust grid offset to make space for the title
            grid_offset_y = y + title_font_size + 30

            # Calculate horizontal offset to center the grid
            centered_grid_x_offset = self.grid_component.calculate_centered_offset_x(
                width
            )

            self.grid_component.update_grid_dimensions(len(self.chat_rooms), 1)
            self.grid_component.render(
                offset_x=x + centered_grid_x_offset,
                offset_y=grid_offset_y,
                container_width=width,
                container_height=height - (grid_offset_y - y),
                items_data=self.chat_rooms,
                mouse_x=mouse_x,
                mouse_y=mouse_y,
                selected_index=self.selected_chat_index,
            )
            # Ensure message input is not active when in list view
            self.message_input.set_active(False)
        else:
            # Render single chat detail view
            selected_chat_data = self.chat_rooms[self.selected_chat_index]
            self._render_single_chat_detail(
                modal_component,
                x,
                y,
                width,
                height,
                selected_chat_data,
                mouse_x,
                mouse_y,
                is_mouse_button_pressed,  # This is the general is_mouse_button_pressed
                is_mouse_button_down,  # Pass new parameter
                char_pressed,
                key_pressed,
                is_key_down_map,
                dt,
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
        Handles clicks within the chat modal. Returns True if a click was handled.
        """
        # If in detail view, scrollbar might handle the click first.
        # The scrollbar's handle_event is called within _render_single_chat_detail.
        # Here, we primarily handle clicks for list view items or the input box.
        # For detail view, if scrollbar consumed it, this function might not need to do much more for that click.
        # However, the input box click check is still relevant.
        mouse_pos = Vector2(float(mouse_x), float(mouse_y))

        # The input field's coordinates are relative to the modal content area
        input_box_x = offset_x + 20
        input_box_y = (
            offset_y + container_height - self.message_input.height - 10
        )  # 10 pixels from bottom

        # Temporarily update input field's position for click check
        self.message_input.x = input_box_x
        self.message_input.y = input_box_y
        self.message_input.width = (
            container_width - 40
        )  # width - 40 for input_box_width

        # Check if the input field was clicked, but only if the click wasn't on an active scrollbar.
        # A more robust solution would be if scrollbar.handle_event returned if it consumed the *press* specifically.
        # For now, we assume if the scrollbar is active and mouse is over it, it might have taken the click.
        clicked_on_active_scrollbar = False
        if (
            self.selected_chat_index is not None
        ):  # Only check scrollbar if in detail view
            if (
                check_collision_point_rec(mouse_pos, self.scrollbar.rect)
                and self.scrollbar.content_height > self.scrollbar.visible_height
            ):
                clicked_on_active_scrollbar = True  # Approximate

        if not clicked_on_active_scrollbar and self.message_input.check_click(
            mouse_x, mouse_y, is_mouse_button_pressed
        ):
            return True  # Input field handled the click (activation/start drag)

        if not is_mouse_button_pressed:
            return False

        if self.selected_chat_index is None:
            # In list view, check for chat room item clicks
            title_font_size = UI_FONT_SIZE + 2
            grid_offset_y = offset_y + title_font_size + 30

            centered_grid_x_offset = self.grid_component.calculate_centered_offset_x(
                container_width
            )

            clicked_index = self.grid_component.get_clicked_item_index(
                offset_x=offset_x + centered_grid_x_offset,
                offset_y=grid_offset_y,
                mouse_x=mouse_x,
                mouse_y=mouse_y,
                is_mouse_button_pressed=is_mouse_button_pressed,
            )

            if clicked_index is not None and clicked_index < len(self.chat_rooms):
                self.selected_chat_index = clicked_index
                logging.info(
                    f"Chat room clicked: {self.chat_rooms[clicked_index].get('name')}"
                )
                self.message_input.set_active(
                    False
                )  # Deactivate input on chat room selection
                self.message_input.set_text(
                    self.message_input_placeholder
                )  # Clear text on chat room change
                self._update_history_dimensions_and_scrollbar(
                    self.chat_rooms[clicked_index].get("messages", [])
                )
                self.scrollbar.set_scroll_position(1.0)  # Scroll to bottom of new chat
                return True
            return False
        else:
            # In single chat detail view, if click was not handled by input field, no other elements are interactive yet
            return False

    def reset_view(self):
        """Resets the view state, e.g., deselects any selected chat room and clears input."""
        self.selected_chat_index = None
        self.title_text = "Chat Rooms"
        self.message_input.set_active(False)
        self.message_input.set_text(
            self.message_input_placeholder
        )  # Use stored placeholder

    def handle_server_chat_message(self, message_data: dict):
        """
        Processes an incoming chat message from the server (via proxy)
        and updates the local chat room data.
        """
        try:
            room_id = message_data["data"]["room_id"]
            sender = message_data["data"]["sender"]
            text = message_data["data"]["text"]
            timestamp = message_data["data"]["time"]

            # Assume network_proxy has a way to get the current client's ID/sender name
            # This needs to match the format of the 'sender' field in message_data
            my_identifier = getattr(
                self.network_proxy, "client_id", None
            )  # Example: self.network_proxy.client_id
            if my_identifier is None:
                # Fallback or log if client_id is not available, affects auto-scroll for self
                logging.warning(
                    "ChatCyberiaView: Client identifier not available on network_proxy for auto-scroll logic."
                )

            is_message_from_self = my_identifier is not None and sender == my_identifier

            for room in self.chat_rooms:
                if room["id"] == room_id:
                    room["messages"].append(
                        {"sender": sender, "text": text, "time": timestamp}
                    )
                    room["last_message"] = text  # Update last message preview
                    # logging.info(
                    #     f"ChatCyberiaView: Received message for room {room_id}: '{text}' from {sender}"
                    # )

                    # If this is the currently viewed room, update scrollbar and auto-scroll if at bottom
                    if (
                        self.selected_chat_index is not None
                        and self.chat_rooms[self.selected_chat_index]["id"] == room_id
                    ):
                        should_scroll_to_bottom = False
                        if is_message_from_self:
                            should_scroll_to_bottom = True
                        else:
                            # For messages from others, scroll only if already at/near the bottom
                            old_content_height = self.scrollbar.content_height
                            old_visible_height = self.scrollbar.visible_height
                            old_scroll_pos = self.scrollbar.scroll_position

                            is_explicitly_at_bottom = (
                                old_scroll_pos >= 0.995
                            )  # Using a slightly tighter epsilon
                            is_content_fitting_or_nearly_fitting = (
                                old_content_height
                                <= old_visible_height
                                + (self.message_line_render_height * 1.5)
                            )  # Content fits or very close to fitting

                            if (
                                is_explicitly_at_bottom
                                or is_content_fitting_or_nearly_fitting
                            ):
                                should_scroll_to_bottom = True

                        self._update_history_dimensions_and_scrollbar(room["messages"])

                        if should_scroll_to_bottom:
                            self.scrollbar.set_scroll_position(1.0)
                            self._scroll_offset_y = (
                                self.scrollbar.get_scroll_offset()
                            )  # Crucial: update offset after changing scroll position
                    return
            logging.warning(
                f"ChatCyberiaView: Received message for unknown room_id: {room_id}"
            )
        except KeyError as e:
            logging.error(
                f"ChatCyberiaView: Error processing server_chat_message - missing key: {e} in {message_data}"
            )
        except Exception as e:
            logging.exception(
                f"ChatCyberiaView: Unexpected error processing server_chat_message: {e}"
            )
