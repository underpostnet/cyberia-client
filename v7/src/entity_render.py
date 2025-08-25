import time
import pyray as pr


class EntityRender:
    def __init__(self, game_state):
        self.game_state = game_state

    def interpolate_entities_positions(self):
        """
        Smoothly interpolate positions for other_players and bots using pos_prev -> pos_server
        and the same interpolation time window used for player (interpolation_ms).
        """
        with self.game_state.mutex:
            interp_ms = (
                self.game_state.interpolation_ms
                if self.game_state.interpolation_ms > 0
                else 200
            )
            max_dt = interp_ms / 1000.0
            now = time.time()

            # other players
            for pid, entry in list(self.game_state.other_players.items()):
                last_update = entry.get("last_update", now)
                # compute factor relative to when server pos was set
                dt = now - last_update
                factor = 1.0 if max_dt <= 0 else min(1.0, dt / max_dt)
                a = entry.get("pos_prev", entry.get("pos_server"))
                b = entry.get("pos_server", a)
                try:
                    nx = pr.lerp(a.x, b.x, factor)
                    ny = pr.lerp(a.y, b.y, factor)
                except Exception:
                    nx, ny = b.x, b.y
                entry["interp_pos"] = pr.Vector2(nx, ny)

            # bots
            for bid, entry in list(self.game_state.bots.items()):
                last_update = entry.get("last_update", now)
                dt = now - last_update
                factor = 1.0 if max_dt <= 0 else min(1.0, dt / max_dt)
                a = entry.get("pos_prev", entry.get("pos_server"))
                b = entry.get("pos_server", a)
                try:
                    nx = pr.lerp(a.x, b.x, factor)
                    ny = pr.lerp(a.y, b.y, factor)
                except Exception:
                    nx, ny = b.x, b.y
                entry["interp_pos"] = pr.Vector2(nx, ny)
