import logging
from raylibpy import Color, Vector2, RAYWHITE
from config import (
    UI_FONT_SIZE,
    UI_TEXT_COLOR_PRIMARY,
    UI_TEXT_COLOR_SHADING,
)
from object_layer.object_layer_render import ObjectLayerRender

# Removed the circular import of BagCyberiaView
from object_layer.object_layer_data import (
    Direction,
    ObjectLayerMode,
)  # Import for animation state

logging.basicConfig(
    level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s"
)

# Explicitly set bag inventory constants
BAG_INVENTORY_ROWS = 5
BAG_INVENTORY_COLS = 6
BAG_SLOT_SIZE = 40
BAG_SLOT_PADDING = 10


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

    # Update modal_component's title to match BagCyberiaView's internal title
    modal_component.set_title(bag_view_instance.title_text)

    # The BagCyberiaView instance now handles its own content rendering logic (grid vs. single item)
    bag_view_instance.render_content(
        x,
        y,
        width,
        height,
        player_object_layer_ids,
        mouse_x,
        mouse_y,
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


def render_modal_object_layer_item_content(
    modal_component,  # This will be a ModalCoreComponent holding object_layer_id_to_render
    object_layer_render_instance: ObjectLayerRender,
    x: int,
    y: int,
    width: int,
    height: int,
    data_to_pass: dict = None,
):
    """
    Renders a specific object layer animation frame within a modal slot,
    typically for inventory display of "skin" items.
    The animation is set to DOWN_WALKING_IDLE and will now animate.
    """
    object_layer_id = modal_component.object_layer_id_to_render  # Access directly
    if not object_layer_id:
        logging.warning(
            "No object_layer_id_to_render specified for modal_object_layer_item_content."
        )
        return

    # Get the object layer definition to determine matrix dimension and other properties
    object_layer_info = object_layer_render_instance.get_object_layer_definition(
        object_layer_id
    )
    if object_layer_info:
        object_layer_info_render_data = object_layer_info.get("RENDER_DATA")
        if not object_layer_info_render_data:
            logging.warning(
                f"No RENDER_DATA found for object layer ID: {object_layer_id}. Cannot render item."
            )
            return
    else:
        logging.warning(
            f"No object layer definition found for ID: {object_layer_id}. Cannot render item."
        )
        return

    matrix_dimension = object_layer_render_instance.get_object_layer_matrix_dimension(
        object_layer_id
    )
    if matrix_dimension == 0:
        logging.warning(
            f"Matrix dimension is 0 for {object_layer_id}. Cannot render item."
        )
        return

    # Calculate pixel size to fit exactly within the slot, using integer division for precision
    pixel_size_in_display = width // matrix_dimension
    if pixel_size_in_display == 0:
        pixel_size_in_display = 1

    # Calculate the total rendered size of the animation
    rendered_width = pixel_size_in_display * matrix_dimension
    rendered_height = pixel_size_in_display * matrix_dimension

    # Calculate offset to center the rendered animation within the slot
    offset_x = (width - rendered_width) // 2
    offset_y = (height - rendered_height) // 2

    # Get or create the animation instance for this specific item in the bag context
    # Use a unique ID for the animation instance to avoid conflicts with world objects
    anim_props = object_layer_render_instance.get_or_create_object_layer_animation(
        obj_id=f"bag_item_{object_layer_id}",
        object_layer_id=object_layer_id,
        target_object_layer_size_pixels=pixel_size_in_display,
        initial_direction=Direction.DOWN,  # Default direction for inventory item display
    )

    anim_instance = anim_props["object_layer_animation_instance"]

    # Set animation to DOWN_WALKING_IDLE. Removed pause_at_frame to allow animation to play.
    anim_instance.set_state(Direction.DOWN, ObjectLayerMode.WALKING, 0.0)

    # Get the frame matrix and color map from the current animation state
    # (it will now advance due to updates in object_layer_render)
    frame_matrix, color_map, _, _ = anim_instance.get_current_frame_data(
        object_layer_render_instance.get_frame_time()
    )

    # Render the frame directly within the slot's bounds, with centering adjustment
    object_layer_render_instance.render_specific_object_layer_frame(
        frame_matrix=frame_matrix,
        color_map=color_map,
        screen_x=x + offset_x,
        screen_y=y + offset_y,
        pixel_size_in_display=pixel_size_in_display,
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
