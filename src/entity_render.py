import time
import pyray as pr
from src.object_layer import Direction, ObjectLayerMode


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

    def _draw_entity_label(self, px, py, text_lines, font_size=12):
        """
        Helper to draw stacked label lines centered horizontally at px..py (py is top of first line).
        text_lines: list of strings, drawn top->down
        """
        y = py
        for line in text_lines:
            tw = pr.measure_text(line, font_size)
            pr.draw_text_ex(
                pr.get_font_default(),
                line,
                pr.Vector2(px - tw / 2, y),
                font_size,
                1,
                self.game_state.colors.get("UI_TEXT", pr.Color(255, 255, 255, 255)),
            )
            y += font_size + 2

    def draw_entities_sorted(self, entity_player_render, entity_bot_render):
        """
        This function draws bots, other players and the local player in a single sorted pass
        by their bottom Y (pos.y + dims.y) using interpolated positions so objects lower on screen render on top.
        Labels are drawn with each entity to avoid z-fighting.
        """
        entries = []
        with self.game_state.mutex:
            # other players
            for player_id, p in self.game_state.other_players.items():
                pos = p.get("interp_pos", p.get("pos_server"))
                dims = p.get("dims", pr.Vector2(1, 1))
                bottom_y = pos.y + dims.y  # measured in grid cells
                entries.append(("other", bottom_y, player_id, p))
            # bots
            for bot_id, b in self.game_state.bots.items():
                pos = b.get("interp_pos", b.get("pos_server"))
                dims = b.get("dims", pr.Vector2(1, 1))
                bottom_y = pos.y + dims.y
                entries.append(("bot", bottom_y, bot_id, b))
            # self player (drawn as an entity too) use interpolated player pos
            self_pos = self.game_state.player_pos_interpolated
            self_dims = self.game_state.player_dims
            bottom_y_self = self_pos.y + self_dims.y
            entries.append(
                (
                    "self",
                    bottom_y_self,
                    self.game_state.player_id,
                    {
                        "pos": self_pos,
                        "dims": self_dims,
                        "direction": self.game_state.player_direction,
                        "mode": self.game_state.player_mode,
                    },
                )
            )

        # sort ascending by bottom_y; smaller Y (higher on map) drawn first
        entries.sort(key=lambda e: e[1])

        # draw in sorted order
        for typ, _, _id, data in entries:

            if typ == "other":
                # data contains interp_pos, dims, direction, mode
                interp_pos = data.get(
                    "interp_pos", data.get("pos_server", pr.Vector2(0, 0))
                )
                dims = data.get("dims", pr.Vector2(1, 1))
                entity_player_render._draw_player_at(
                    interp_pos,
                    dims,
                    False,
                    data.get("direction", Direction.NONE),
                    data.get("mode", ObjectLayerMode.IDLE),
                    entity_id=_id,
                )
            elif typ == "bot":
                entity_bot_render._draw_bot_at(data, bot_id=_id)
            elif typ == "self":
                entity_player_render._draw_player_at(
                    data["pos"],
                    data["dims"],
                    True,
                    data.get("direction", Direction.NONE),
                    data.get("mode", ObjectLayerMode.IDLE),
                    entity_id=_id or "you",
                )
