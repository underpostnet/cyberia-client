import logging
from raylibpy import Color
from config import UI_FONT_SIZE, UI_TEXT_COLOR_PRIMARY, UI_TEXT_COLOR_SHADING
from ui.components.core.grid_core_component import GridCoreComponent
from object_layer.object_layer_render import ObjectLayerRender

logging.basicConfig(
    level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s"
)


# Example quest data structure
QUEST_DATA = [
    {
        "id": "quest_001",
        "title": "The Missing Spark",
        "description": "A local bot's power core has gone missing. Find it! Go to the abandoned factory in Sector 4 and retrieve the core. Be careful, it's heavily guarded by rogue bots.",
        "status": "active",
        "reward": "50 Credits",
    },
    {
        "id": "quest_002",
        "title": "Data Recovery",
        "description": "Retrieve corrupted data from Sector Gamma-7. Beware of glitches. The data is critical for the city's power grid. You'll need a specialized data-siphon tool.",
        "status": "completed",
        "reward": "Rare Chip",
    },
    {
        "id": "quest_003",
        "title": "A Friend in Need",
        "description": "Help the lost Traveler bot find its way home through the maze. The bot is stranded in the winding alleys of the Slums. Lead it safely back to the city hub.",
        "status": "active",
        "reward": "Navigation Module",
    },
    {
        "id": "quest_004",
        "title": "Scavenger Hunt",
        "description": "Collect 10 energy cells scattered across the world. These cells are vital for upgrading your personal shield. Check old wreckage sites and abandoned power stations.",
        "status": "inactive",
        "reward": "Energy Pack",
    },
    {
        "id": "quest_005",
        "title": "Defensive Measures",
        "description": "Install defensive turrets around the central hub. The city is under constant threat from raiders. Your engineering skills are needed to fortify our defenses.",
        "status": "active",
        "reward": "Heavy Plating",
    },
    {
        "id": "quest_006",
        "title": "System Reboot",
        "description": "Access the main server and initiate a system-wide reboot. The network is sluggish and causing widespread malfunctions. A hard reset is the only option.",
        "status": "inactive",
        "reward": "Master Key",
    },
]


class QuestCyberiaView:
    """
    Manages the display and interaction for the quest list interface.
    Uses GridCoreComponent to render quests in a list bar format.
    """

    def __init__(self, object_layer_render_instance: ObjectLayerRender):
        self.object_layer_render = object_layer_render_instance
        self.title_text = "Quests"
        self.quests = QUEST_DATA
        self.selected_quest_index: int | None = None  # To track selected quest

        # Configure GridCoreComponent for a list bar form (1 column, multiple rows)
        self.grid_component = GridCoreComponent(
            object_layer_render_instance=self.object_layer_render,
            num_rows=len(self.quests),  # Number of rows based on number of quests
            num_cols=1,  # Single column for list bar
            item_width=280,  # Width of each quest item
            item_height=80,  # Height of each quest item
            item_padding=5,  # Padding between quest items
            render_item_callback=self._render_quest_item,
            background_color=Color(
                0, 0, 0, 0
            ),  # Transparent background, main modal draws it
            border_color=Color(50, 50, 50, 200),
            slot_background_color=Color(30, 30, 30, 180),
            slot_hover_color=Color(50, 50, 50, 200),
            slot_selected_color=Color(
                150, 150, 0, 200
            ),  # More prominent selected color
        )

    def _render_quest_item(
        self,
        object_layer_render_instance: ObjectLayerRender,
        x: int,
        y: int,
        width: int,
        height: int,
        quest_data: dict,
        is_hovered: bool,
        is_selected: bool,
    ):
        """
        Renders the content of a single quest item within the grid slot.
        This is a callback passed to GridCoreComponent.
        """
        text_color = Color(*UI_TEXT_COLOR_PRIMARY)
        shading_color = Color(*UI_TEXT_COLOR_SHADING)

        title = quest_data.get("title", "No Title")
        status = quest_data.get("status", "unknown").upper()
        reward = quest_data.get("reward", "None")

        # Draw Title
        object_layer_render_instance.draw_text(
            title, x + 10 + 1, y + 10 + 1, UI_FONT_SIZE, shading_color
        )
        object_layer_render_instance.draw_text(
            title, x + 10, y + 10, UI_FONT_SIZE, text_color
        )

        # Draw Status
        status_text = f"Status: {status}"
        status_color = Color(0, 255, 0, 255) if status == "COMPLETED" else text_color
        if status == "INACTIVE":
            status_color = Color(150, 150, 150, 255)  # Gray for inactive

        object_layer_render_instance.draw_text(
            status_text, x + 10 + 1, y + 30 + 1, UI_FONT_SIZE - 2, shading_color
        )
        object_layer_render_instance.draw_text(
            status_text, x + 10, y + 30, UI_FONT_SIZE - 2, status_color
        )

        # Draw Reward
        reward_text = f"Reward: {reward}"
        object_layer_render_instance.draw_text(
            reward_text, x + 10 + 1, y + 50 + 1, UI_FONT_SIZE - 2, shading_color
        )
        object_layer_render_instance.draw_text(
            reward_text, x + 10, y + 50, UI_FONT_SIZE - 2, text_color
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

    def _render_single_quest_detail(
        self,
        modal_component,  # Added modal_component to access its set_title method
        x: int,
        y: int,
        width: int,
        height: int,
        quest_data: dict,
        mouse_x: int,
        mouse_y: int,
        is_mouse_button_pressed: bool,
    ):
        """
        Renders the detailed view of a single selected quest.
        """
        text_color = Color(*UI_TEXT_COLOR_PRIMARY)
        shading_color = Color(*UI_TEXT_COLOR_SHADING)

        title = quest_data.get("title", "No Title")
        description = quest_data.get("description", "No description available.")
        status = quest_data.get("status", "unknown").upper()
        reward = quest_data.get("reward", "None")

        self.title_text = (
            title  # Update QuestCyberiaView's internal title (for modal to pick up)
        )
        modal_component.set_title(self.title_text)  # Set the modal's title

        # Draw Title
        title_width = self.object_layer_render.measure_text(title, UI_FONT_SIZE + 5)
        self.object_layer_render.draw_text(
            title,
            x + (width - title_width) // 2 + 1,
            y + 20 + 1,
            UI_FONT_SIZE + 5,
            shading_color,
        )
        self.object_layer_render.draw_text(
            title, x + (width - title_width) // 2, y + 20, UI_FONT_SIZE + 5, text_color
        )

        # Draw Description (multiline)
        desc_start_y = y + 70
        max_desc_width = width - 40  # 20px padding on each side
        lines = self._wrap_text(description, max_desc_width, UI_FONT_SIZE - 2)

        current_y = desc_start_y
        for line in lines:
            self.object_layer_render.draw_text(
                line, x + 20 + 1, current_y + 1, UI_FONT_SIZE - 2, shading_color
            )
            self.object_layer_render.draw_text(
                line, x + 20, current_y, UI_FONT_SIZE - 2, text_color
            )
            current_y += (UI_FONT_SIZE - 2) + 5  # Line height + spacing

        # Draw Status and Reward below description
        status_text = f"Status: {status}"
        status_color = Color(0, 255, 0, 255) if status == "COMPLETED" else text_color
        if status == "INACTIVE":
            status_color = Color(150, 150, 150, 255)  # Gray for inactive

        self.object_layer_render.draw_text(
            status_text, x + 20 + 1, current_y + 10 + 1, UI_FONT_SIZE, shading_color
        )
        self.object_layer_render.draw_text(
            status_text, x + 20, current_y + 10, UI_FONT_SIZE, status_color
        )

        reward_text = f"Reward: {reward}"
        self.object_layer_render.draw_text(
            reward_text,
            x + 20 + 1,
            current_y + 10 + UI_FONT_SIZE + 5 + 1,
            UI_FONT_SIZE,
            shading_color,
        )
        self.object_layer_render.draw_text(
            reward_text,
            x + 20,
            current_y + 10 + UI_FONT_SIZE + 5,
            UI_FONT_SIZE,
            text_color,
        )

        # Example "Accept/Complete Quest" button (logic placeholder)
        button_width = 150
        button_height = 40
        button_x = x + (width - button_width) // 2
        button_y = height - button_height - 20  # Position near bottom

        button_color = Color(0, 121, 241, 200)  # Blueish
        button_text = "Accept Quest"
        if status == "ACTIVE":
            button_text = "Complete Quest"
            button_color = Color(0, 180, 0, 200)  # Green for active
        elif status == "COMPLETED":
            button_text = "Claim Reward"
            button_color = Color(100, 100, 100, 150)  # Gray for completed

        # Handle button hover
        is_button_hovered = (
            mouse_x >= button_x
            and mouse_x <= (button_x + button_width)
            and mouse_y >= button_y
            and mouse_y <= (button_y + button_height)
        )

        if is_button_hovered and status != "COMPLETED":
            button_color.a = 255  # Make more opaque on hover

        self.object_layer_render.draw_rectangle(
            button_x, button_y, button_width, button_height, button_color
        )
        btn_text_width = self.object_layer_render.measure_text(
            button_text, UI_FONT_SIZE
        )
        self.object_layer_render.draw_text(
            button_text,
            button_x + (button_width - btn_text_width) // 2 + 1,
            button_y + (button_height - UI_FONT_SIZE) // 2 + 1,
            UI_FONT_SIZE,
            Color(*UI_TEXT_COLOR_SHADING),
        )
        self.object_layer_render.draw_text(
            button_text,
            button_x + (button_width - btn_text_width) // 2,
            button_y + (button_height - UI_FONT_SIZE) // 2,
            UI_FONT_SIZE,
            Color(*UI_TEXT_COLOR_PRIMARY),
        )

        # This function doesn't handle clicks directly, it only renders.
        # Click handling for this button will be done in handle_item_clicks

    def _wrap_text(self, text: str, max_width: int, font_size: int) -> list[str]:
        """Wraps text to fit within a maximum width."""
        words = text.split(" ")
        lines = []
        current_line = []
        for word in words:
            # Temporarily join with space to measure, then remove if too long
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
        modal_component,  # Added modal_component
        x: int,
        y: int,
        width: int,
        height: int,
        mouse_x: int,
        mouse_y: int,
        is_mouse_button_pressed: bool,
    ):
        """
        Renders the quest list view or a single quest detail view.
        Args:
            x, y, width, height: Dimensions of the modal container.
            mouse_x, mouse_y: Current mouse coordinates.
            is_mouse_button_pressed: True if the mouse button is pressed.
        """
        if self.selected_quest_index is None:
            # Render the quest list
            self.title_text = "Quests"  # Reset internal title for list view
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
            grid_offset_y = y + title_font_size + 30  # 30 pixels padding after title

            # Calculate horizontal offset to center the grid using the new method
            centered_grid_x_offset = self.grid_component.calculate_centered_offset_x(
                width
            )

            self.grid_component.update_grid_dimensions(len(self.quests), 1)
            self.grid_component.render(
                offset_x=x + centered_grid_x_offset,  # Apply centering offset
                offset_y=grid_offset_y,
                container_width=width,
                container_height=height
                - (
                    grid_offset_y - y
                ),  # Reduce container height by the space taken by title
                items_data=self.quests,
                mouse_x=mouse_x,
                mouse_y=mouse_y,
                selected_index=self.selected_quest_index,
            )
        else:
            # Render single quest detail view
            selected_quest_data = self.quests[self.selected_quest_index]
            self._render_single_quest_detail(
                modal_component,  # Pass modal_component
                x,
                y,
                width,
                height,
                selected_quest_data,
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
        Handles clicks within the quest modal. Returns True if a click was handled.
        """
        if not is_mouse_button_pressed:
            return False

        if self.selected_quest_index is None:
            # In list view, check for quest item clicks
            title_font_size = UI_FONT_SIZE + 2
            grid_offset_y = offset_y + title_font_size + 30

            centered_grid_x_offset = self.grid_component.calculate_centered_offset_x(
                container_width
            )

            clicked_index = self.grid_component.get_clicked_item_index(
                offset_x=offset_x + centered_grid_x_offset,  # Apply centering offset
                offset_y=grid_offset_y,  # Use adjusted offset for click detection
                mouse_x=mouse_x,
                mouse_y=mouse_y,
                is_mouse_button_pressed=is_mouse_button_pressed,
            )

            if clicked_index is not None and clicked_index < len(self.quests):
                self.selected_quest_index = clicked_index
                logging.info(
                    f"Quest clicked: {self.quests[clicked_index].get('title')}"
                )
                return True
            return False
        else:
            # In single quest detail view, check for "Accept/Complete/Claim" button click
            selected_quest_data = self.quests[self.selected_quest_index]
            status = selected_quest_data.get("status", "unknown").upper()

            button_width = 150
            button_height = 40
            button_x = offset_x + (container_width - button_width) // 2
            button_y = container_height - button_height - 20  # From bottom of modal

            if (
                mouse_x >= button_x
                and mouse_x <= (button_x + button_width)
                and mouse_y >= button_y
                and mouse_y <= (button_y + button_height)
                and status != "COMPLETED"  # Cannot click if already completed
            ):
                logging.info(
                    f"Action button clicked for quest: {selected_quest_data.get('title')}"
                )
                # Placeholder for quest action logic
                if status == "ACTIVE":
                    logging.info("Attempting to complete quest...")
                    # self.quests[self.selected_quest_index]["status"] = "completed" # Example state change
                elif status == "INACTIVE":
                    logging.info("Accepting quest...")
                    # self.quests[self.selected_quest_index]["status"] = "active" # Example state change

                # In a real game, this would send a message to the server.
                return True  # Indicate click handled
            return False

    def reset_view(self):
        """Resets the view state, e.g., deselects any selected quest."""
        self.selected_quest_index = None
        self.title_text = "Quests"
