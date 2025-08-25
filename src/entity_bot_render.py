import pyray as pr
from src.object_layer import Direction, ObjectLayerMode


class EntityBotRender:
    def __init__(self, game_state, entity_render):
        self.game_state = game_state
        self.entity_render = entity_render

    def _draw_bot_at(self, bot_entry, bot_id=None):
        """
        bot_entry is the dict with fields 'interp_pos','dims','behavior','direction'
        """
        cell_size = self.game_state.cell_size if self.game_state.cell_size > 0 else 12.0
        pos = bot_entry.get("interp_pos", bot_entry.get("pos_server", pr.Vector2(0, 0)))
        dims = bot_entry.get("dims", pr.Vector2(1, 1))
        behavior = bot_entry.get("behavior", "passive")
        direction = bot_entry.get("direction", Direction.NONE)

        scaled_pos_x = pos.x * cell_size
        scaled_pos_y = pos.y * cell_size
        scaled_w = dims.x * cell_size
        scaled_h = dims.y * cell_size

        # choose color based on behavior
        if behavior == "hostile":
            color_bot = self.game_state.colors.get(
                "ERROR_TEXT", pr.Color(255, 50, 50, 255)
            )
        else:
            color_bot = self.game_state.colors.get(
                "OTHER_PLAYER", pr.Color(100, 200, 100, 255)
            )

        # Draw label stacked above the bot: ID, Direction, Behavior
        center_x = scaled_pos_x + scaled_w / 2.0
        label_top_y = scaled_pos_y - 44
        id_text = bot_id if bot_id is not None else "bot"
        dir_text = (
            direction.name if isinstance(direction, Direction) else str(direction)
        )
        type_text = behavior

        self.entity_render._draw_entity_label(
            center_x,
            label_top_y,
            [str(id_text), str(dir_text), str(type_text)],
            font_size=12,
        )

        pr.draw_rectangle_pro(
            pr.Rectangle(scaled_pos_x, scaled_pos_y, scaled_w, scaled_h),
            pr.Vector2(0, 0),
            0,
            color_bot,
        )
