import time

import pyray as pr

from src.object_layer.object_layer import Direction, ObjectLayerMode


class EntityPlayerRender:
    def __init__(self, game_state, entity_render):
        self.game_state = game_state
        self.entity_render = entity_render

    def interpolate_player_position(self):
        print(
            "[DEBUG INTERP] interpolate_player_position() called, attempting to acquire mutex..."
        )
        # Try to acquire mutex with timeout to prevent indefinite freeze
        acquired = self.game_state.mutex.acquire(timeout=0.1)
        if not acquired:
            print(
                "[DEBUG INTERP] ⚠️ WARNING: Could not acquire mutex for player interpolation (timeout), skipping this frame"
            )
            return

        try:
            print("[DEBUG INTERP] Mutex acquired for player interpolation")

            # Check if player object exists
            if not self.game_state.player:
                print(
                    "[DEBUG INTERP] ⚠️ WARNING: player is None, skipping interpolation"
                )
                return

            # Check if position vectors exist
            if (
                not self.game_state.player.pos_prev
                or not self.game_state.player.pos_server
            ):
                print(
                    f"[DEBUG INTERP] ⚠️ WARNING: player positions not initialized "
                    f"(pos_prev={self.game_state.player.pos_prev}, pos_server={self.game_state.player.pos_server}), skipping interpolation"
                )
                return

            interp_ms = (
                self.game_state.interpolation_ms
                if self.game_state.interpolation_ms > 0
                else 200
            )
            print(f"[DEBUG INTERP] interp_ms={interp_ms}")
            time_since_update = time.time() - self.game_state.last_update_time
            interp_factor = min(1.0, time_since_update / (interp_ms / 1000.0))
            print(
                f"[DEBUG INTERP] time_since_update={time_since_update:.4f}, interp_factor={interp_factor:.4f}"
            )

            current_x = pr.lerp(
                self.game_state.player.pos_prev.x,
                self.game_state.player.pos_server.x,
                interp_factor,
            )
            current_y = pr.lerp(
                self.game_state.player.pos_prev.y,
                self.game_state.player.pos_server.y,
                interp_factor,
            )
            print(
                f"[DEBUG INTERP] Interpolated position: ({current_x:.2f}, {current_y:.2f})"
            )
            self.game_state.player.interp_pos = pr.Vector2(current_x, current_y)
            print("[DEBUG INTERP] Player interpolation completed, releasing mutex")
        except Exception as e:
            print(
                f"[DEBUG INTERP] ❌ ERROR in interpolate_player_position: {type(e).__name__}: {e}"
            )
            import traceback

            traceback.print_exc()
        finally:
            self.game_state.mutex.release()

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
