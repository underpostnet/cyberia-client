import time
import pyray as pr
from src.object_layer.object_layer import Direction, ObjectLayerMode


class EntityPlayerRender:
    def __init__(self, game_state, entity_render):
        self.game_state = game_state
        self.entity_render = entity_render

    def interpolate_player_position(self):
        with self.game_state.mutex:
            interp_ms = (
                self.game_state.interpolation_ms
                if self.game_state.interpolation_ms > 0
                else 200
            )
            time_since_update = time.time() - self.game_state.last_update_time
            interp_factor = min(1.0, time_since_update / (interp_ms / 1000.0))

            current_x = pr.lerp(
                self.game_state.player_pos_prev.x,
                self.game_state.player_pos_server.x,
                interp_factor,
            )
            current_y = pr.lerp(
                self.game_state.player_pos_prev.y,
                self.game_state.player_pos_server.y,
                interp_factor,
            )
            self.game_state.player_pos_interpolated = pr.Vector2(current_x, current_y)

    def _draw_player_at(
        self,
        pos_vec,
        dims_vec,
        is_self=False,
        direction=Direction.NONE,
        mode=ObjectLayerMode.IDLE,
        entity_id=None,
    ):
        cell_size = self.game_state.cell_size if self.game_state.cell_size > 0 else 12.0
        scaled_pos_x = pos_vec.x * cell_size
        scaled_pos_y = pos_vec.y * cell_size
        scaled_dims_w = dims_vec.x * cell_size
        scaled_dims_h = dims_vec.y * cell_size
        color_player = (
            self.game_state.colors.get("PLAYER", pr.Color(0, 200, 255, 255))
            if is_self
            else self.game_state.colors.get("OTHER_PLAYER", pr.Color(255, 100, 0, 255))
        )
