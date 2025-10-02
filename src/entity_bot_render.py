import pyray as pr
from src.object_layer.object_layer import Direction, ObjectLayerMode


class EntityBotRender:
    def __init__(self, game_state, entity_render):
        self.game_state = game_state
        self.entity_render = entity_render

    def _draw_bot_at(self, bot_entry, bot_id=None):
        """
        bot_entry is the dict with fields 'interp_pos','dims','behavior','direction'
        """
        cell_size = self.game_state.cell_size if self.game_state.cell_size > 0 else 12.0
        pos = bot_entry.interp_pos
        dims = bot_entry.dims
        behavior = bot_entry.behavior
        direction = bot_entry.direction

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
