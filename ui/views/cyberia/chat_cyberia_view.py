import logging
from raylibpy import Color
from config import UI_FONT_SIZE, UI_TEXT_COLOR_PRIMARY, UI_TEXT_COLOR_SHADING
from ui.components.core.grid_core_component import GridCoreComponent
from object_layer.object_layer_render import ObjectLayerRender

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

    def __init__(self, object_layer_render_instance: ObjectLayerRender):
        self.object_layer_render = object_layer_render_instance
        self.title_text = "Chat Rooms"
        self.chat_rooms = CHAT_ROOM_DATA
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
        is_mouse_button_pressed: bool,
    ):
        """
        Renders the detailed view of a single selected chat room.
        """
        text_color = Color(*UI_TEXT_COLOR_PRIMARY)
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
        desc_start_y = y + 70
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

        # Draw Chat Messages
        current_y += 20  # Add some space after description
        self.object_layer_render.draw_text(
            "Messages:", x + 20 + 1, current_y + 1, UI_FONT_SIZE, shading_color
        )
        self.object_layer_render.draw_text(
            "Messages:", x + 20, current_y, UI_FONT_SIZE, text_color
        )
        current_y += UI_FONT_SIZE + 5

        message_display_limit = 5  # Display last 5 messages for brevity
        for msg in messages[-message_display_limit:]:
            msg_text = f"[{msg.get('time')}] {msg.get('sender')}: {msg.get('text')}"
            self.object_layer_render.draw_text(
                msg_text, x + 20 + 1, current_y + 1, UI_FONT_SIZE - 4, shading_color
            )
            self.object_layer_render.draw_text(
                msg_text, x + 20, current_y, UI_FONT_SIZE - 4, text_color
            )
            current_y += (UI_FONT_SIZE - 4) + 2

        # Placeholder for message input/send functionality (future)
        input_text = "Type your message..."
        input_box_y = height - 60
        input_box_height = 40
        self.object_layer_render.draw_rectangle(
            x + 20, input_box_y, width - 40, input_box_height, Color(50, 50, 50, 200)
        )
        self.object_layer_render.draw_text(
            input_text,
            x + 30 + 1,
            input_box_y + (input_box_height - UI_FONT_SIZE) // 2 + 1,
            UI_FONT_SIZE,
            shading_color,
        )
        self.object_layer_render.draw_text(
            input_text,
            x + 30,
            input_box_y + (input_box_height - UI_FONT_SIZE) // 2,
            UI_FONT_SIZE,
            text_color,
        )

    def _wrap_text(self, text: str, max_width: int, font_size: int) -> list[str]:
        """Wraps text to fit within a maximum width."""
        words = text.split(" ")
        lines = []
        current_line = []
        for word in words:
            test_line = " ".join(current_line + [word])
            if self.object_layer_render.measure_text(test_line, font_size) <= max_width:
                current_line.append(word)
            else:
                lines.append(" ".join(current_line))
                current_line = [word]
        if current_line:
            lines.append(" ".join(current_line))
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
            is_mouse_button_pressed: True if the mouse button is pressed.
        """
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
                is_mouse_button_pressed,
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
                return True
            return False
        else:
            # In single chat detail view, currently no interactive elements
            return False  # No specific button clicks to handle yet

    def reset_view(self):
        """Resets the view state, e.g., deselects any selected chat room."""
        self.selected_chat_index = None
        self.title_text = "Chat Rooms"
