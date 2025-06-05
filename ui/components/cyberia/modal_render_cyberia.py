import logging
from raylibpy import Color, Vector2, RAYWHITE
from config import UI_FONT_SIZE, UI_TEXT_COLOR_PRIMARY, UI_TEXT_COLOR_SHADING
from object_layer.object_layer_render import ObjectLayerRender
from ui.views.cyberia.bag_cyberia_view import BagCyberiaView

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
):
    """
    Renders content specific to the bag view modal.
    Delegates rendering to the BagCyberiaView class.
    """
    BagCyberiaView.render_content(object_layer_render_instance, x, y, width, height)


def render_modal_close_btn_content(
    modal_component,
    object_layer_render_instance: ObjectLayerRender,
    x: int,
    y: int,
    width: int,
    height: int,
):
    """
    Renders content specific to the close button modal.
    Draws the close icon within the button's bounds.
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
        # Fallback text if icon not loaded
        modal_text = "X"
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
        modal_text = "BTN"
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
