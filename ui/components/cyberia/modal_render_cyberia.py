import logging
from raylibpy import Color, Vector2, RAYWHITE
from config import (
    UI_FONT_SIZE,
    UI_TEXT_COLOR_PRIMARY,
    UI_TEXT_COLOR_SHADING,
)
from object_layer.object_layer_render import ObjectLayerRender

# Removed the circular import of BagCyberiaView, will import locally when needed
from object_layer.object_layer_data import (
    Direction,
    ObjectLayerMode,
)  # Import for animation state

logging.basicConfig(
    level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s"
)


def render_modal_quest_discovery_content(
    modal_component,
    object_layer_render_instance: ObjectLayerRender,
    x: int,
    y: int,
    width: int,
    height: int,
    data_to_pass: dict = None,
):
    """
    Renders content specific to the quest discovery modal.
    """
    modal_text = "Quest Available!"
    text_width = object_layer_render_instance.measure_text(modal_text, UI_FONT_SIZE)

    # Center the text within the modal
    text_x = x + (width - text_width) // 2
    text_y = y + (height - UI_FONT_SIZE) // 2

    object_layer_render_instance.draw_text(
        modal_text,
        text_x + 1,
        text_y + 1,
        UI_FONT_SIZE,
        Color(*UI_TEXT_COLOR_SHADING),
    )
    object_layer_render_instance.draw_text(
        modal_text,
        text_x,
        text_y,
        UI_FONT_SIZE,
        Color(*UI_TEXT_COLOR_PRIMARY),
    )


def render_modal_bag_view_content(
    modal_component,
    object_layer_render_instance: ObjectLayerRender,
    x: int,
    y: int,
    width: int,
    height: int,
    data_to_pass: dict = None,
):
    """
    Renders content specific to the bag view modal.
    Delegates rendering to the BagCyberiaView instance, passing player's object layer IDs
    and current mouse coordinates.
    """
    # Import BagCyberiaView locally to avoid circular dependency at top-level
    from ui.views.cyberia.bag_cyberia_view import BagCyberiaView

    bag_view_instance: BagCyberiaView = data_to_pass.get("bag_view_instance")
    if not bag_view_instance:
        logging.error(
            "BagCyberiaView instance not passed to render_modal_bag_view_content."
        )
        return

    player_object_layer_ids = data_to_pass.get("player_object_layer_ids", [])
    mouse_x = data_to_pass.get("mouse_x", -1)
    mouse_y = data_to_pass.get("mouse_y", -1)
    is_mouse_button_pressed = data_to_pass.get("is_mouse_button_pressed", False)

    # The BagCyberiaView instance now handles its own content rendering logic (grid vs. single item)
    # and also sets the modal_component's title.
    bag_view_instance.render_content(
        modal_component,  # Pass modal_component to the view
        x,
        y,
        width,
        height,
        player_object_layer_ids,
        mouse_x,
        mouse_y,
        is_mouse_button_pressed,
    )


def render_modal_quest_list_content(
    modal_component,
    object_layer_render_instance: ObjectLayerRender,
    x: int,
    y: int,
    width: int,
    height: int,
    data_to_pass: dict = None,
):
    """
    Renders content specific to the quest list modal.
    Delegates rendering to the QuestCyberiaView instance.
    """
    from ui.views.cyberia.quest_cyberia_view import QuestCyberiaView

    quest_view_instance: QuestCyberiaView = data_to_pass.get("quest_view_instance")
    if not quest_view_instance:
        logging.error(
            "QuestCyberiaView instance not passed to render_modal_quest_list_content."
        )
        return

    mouse_x = data_to_pass.get("mouse_x", -1)
    mouse_y = data_to_pass.get("mouse_y", -1)
    is_mouse_button_pressed = data_to_pass.get("is_mouse_button_pressed", False)

    quest_view_instance.render_content(
        modal_component,  # Pass modal_component to the view
        x,
        y,
        width,
        height,
        mouse_x,
        mouse_y,
        is_mouse_button_pressed,
    )


def render_modal_character_view_content(
    modal_component,
    object_layer_render_instance: ObjectLayerRender,
    x: int,
    y: int,
    width: int,
    height: int,
    data_to_pass: dict = None,
):
    """
    Renders content specific to the character view modal.
    """
    modal_component.set_title("Character")
    modal_text = "Character View (Under Construction)"
    text_width = object_layer_render_instance.measure_text(modal_text, UI_FONT_SIZE)

    text_x = x + (width - text_width) // 2
    text_y = y + (height - UI_FONT_SIZE) // 2

    object_layer_render_instance.draw_text(
        modal_text,
        text_x + 1,
        text_y + 1,
        UI_FONT_SIZE,
        Color(*UI_TEXT_COLOR_SHADING),
    )
    object_layer_render_instance.draw_text(
        modal_text,
        text_x,
        text_y,
        UI_FONT_SIZE,
        Color(*UI_TEXT_COLOR_PRIMARY),
    )


def render_modal_chat_view_content(
    modal_component,
    object_layer_render_instance: ObjectLayerRender,
    x: int,
    y: int,
    width: int,
    height: int,
    data_to_pass: dict = None,
):
    """
    Renders content specific to the chat view modal.
    """
    modal_component.set_title("Chat")
    modal_text = "Chat View (Under Construction)"
    text_width = object_layer_render_instance.measure_text(modal_text, UI_FONT_SIZE)

    text_x = x + (width - text_width) // 2
    text_y = y + (height - UI_FONT_SIZE) // 2

    object_layer_render_instance.draw_text(
        modal_text,
        text_x + 1,
        text_y + 1,
        UI_FONT_SIZE,
        Color(*UI_TEXT_COLOR_SHADING),
    )
    object_layer_render_instance.draw_text(
        modal_text,
        text_x,
        text_y,
        UI_FONT_SIZE,
        Color(*UI_TEXT_COLOR_PRIMARY),
    )


def render_modal_map_view_content(
    modal_component,
    object_layer_render_instance: ObjectLayerRender,
    x: int,
    y: int,
    width: int,
    height: int,
    data_to_pass: dict = None,
):
    """
    Renders content specific to the map view modal.
    """
    modal_component.set_title("Map")
    modal_text = "Map View (Under Construction)"
    text_width = object_layer_render_instance.measure_text(modal_text, UI_FONT_SIZE)

    text_x = x + (width - text_width) // 2
    text_y = y + (height - UI_FONT_SIZE) // 2

    object_layer_render_instance.draw_text(
        modal_text,
        text_x + 1,
        text_y + 1,
        UI_FONT_SIZE,
        Color(*UI_TEXT_COLOR_SHADING),
    )
    object_layer_render_instance.draw_text(
        modal_text,
        text_x,
        text_y,
        UI_FONT_SIZE,
        Color(*UI_TEXT_COLOR_PRIMARY),
    )


def render_modal_close_btn_content(
    modal_component,
    object_layer_render_instance: ObjectLayerRender,
    x: int,
    y: int,
    width: int,
    height: int,
    data_to_pass: dict = None,
):
    """
    Renders content specific to the close button modal or any button with an icon.
    Draws the icon within the button's bounds or fallback text.
    """
    icon_texture = modal_component.icon_texture
    current_padding = (
        2 if modal_component.is_hovered else 4
    )  # Dynamic padding for hover

    if icon_texture:
        _draw_icon_in_modal(
            object_layer_render_instance,
            x,
            y,
            width,
            height,
            icon_texture,
            current_padding,
        )
    else:
        # Fallback text if icon not loaded, use modal's title text
        modal_text = modal_component.title_text if modal_component.title_text else "X"
        text_width = object_layer_render_instance.measure_text(modal_text, UI_FONT_SIZE)
        text_x = x + (width - text_width) // 2
        text_y = y + (height - UI_FONT_SIZE) // 2
        object_layer_render_instance.draw_text(
            modal_text,
            text_x + 1,
            text_y + 1,
            UI_FONT_SIZE,
            Color(*UI_TEXT_COLOR_SHADING),
        )
        object_layer_render_instance.draw_text(
            modal_text, text_x, text_y, UI_FONT_SIZE, Color(*UI_TEXT_COLOR_PRIMARY)
        )


def render_modal_btn_icon_content(
    modal_component,
    object_layer_render_instance: ObjectLayerRender,
    x: int,
    y: int,
    width: int,
    height: int,
    data_to_pass: dict = None,
):
    """
    Renders content specific to the modal button icons, including the icon image.
    """
    icon_texture = modal_component.icon_texture
    current_padding = 2 if modal_component.is_hovered else 4  # Dynamic padding

    if icon_texture:
        _draw_icon_in_modal(  # Call internal helper method
            object_layer_render_instance,
            x,
            y,
            width,
            height,
            icon_texture,
            current_padding,  # Pass the dynamic padding
        )
    else:
        # Fallback to placeholder text if icon not loaded
        modal_text = modal_component.title_text if modal_component.title_text else "BTN"
        text_width = object_layer_render_instance.measure_text(modal_text, UI_FONT_SIZE)
        text_x = x + (width - text_width) // 2
        text_y = y + (height - UI_FONT_SIZE) // 2
        object_layer_render_instance.draw_text(
            modal_text,
            text_x + 1,
            text_y + 1,
            UI_FONT_SIZE,
            Color(*UI_TEXT_COLOR_SHADING),
        )
        object_layer_render_instance.draw_text(
            modal_text,
            text_x,
            text_y,
            UI_FONT_SIZE,
            Color(*UI_TEXT_COLOR_PRIMARY),
        )


def _draw_icon_in_modal(
    object_layer_render_instance: ObjectLayerRender,
    x: int,
    y: int,
    width: int,
    height: int,
    icon_texture,
    padding: int,
):
    """
    Helper function to draw and center an icon within the modal's bounds with dynamic padding.
    This is an internal helper for modal_render_cyberia and not exposed directly for callbacks.
    """
    if not icon_texture:
        return  # Do nothing if no texture is provided

    # Calculate effective drawing area considering padding
    effective_width = width - 2 * padding
    effective_height = height - 2 * padding

    if effective_width <= 0 or effective_height <= 0:
        logging.warning(
            "Effective drawing area for icon is too small. Skipping icon render."
        )
        return

    # Check if icon_texture has valid dimensions before division
    if icon_texture.width <= 0 or icon_texture.height <= 0:
        logging.warning(
            f"Icon texture has zero or negative dimensions ({icon_texture.width}x{icon_texture.height}). Skipping icon render."
        )
        return

    # Determine scaling factor to fit the icon within the effective area while maintaining aspect ratio
    scale_x = effective_width / icon_texture.width
    scale_y = effective_height / icon_texture.height
    scale_factor = min(scale_x, scale_y)

    # Calculate scaled icon dimensions
    scaled_icon_width = icon_texture.width * scale_factor
    scaled_icon_height = icon_texture.height * scale_factor

    # Calculate position to center the scaled icon within the padded area
    draw_x = x + padding + (effective_width - scaled_icon_width) / 2
    draw_y = y + padding + (effective_height - scaled_icon_height) / 2

    # Draw the texture with scaling using the object_layer_render_instance
    object_layer_render_instance.draw_texture_ex(
        icon_texture, Vector2(draw_x, draw_y), 0.0, scale_factor, RAYWHITE
    )
