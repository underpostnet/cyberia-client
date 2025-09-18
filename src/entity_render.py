import time
import pyray as pr
from src.object_layer import Direction, ObjectLayerMode


class EntityRender:
    def __init__(self, game_state, obj_layers_mgr, texture_manager):
        self.game_state = game_state
        self.obj_layers_mgr = obj_layers_mgr
        self.texture_manager = texture_manager
        # Cache for animation state:
        # (entity_id, item_id) -> {frame_index, last_update_time, last_state_string, last_facing_direction}
        self.animation_state_cache = {}

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

    def _draw_entity_life_bar(self, px, py, width, life, max_life):
        """
        Helper to draw a life bar centered horizontally at px, with text.
        py is the top of the bar.
        """
        if max_life <= 0:
            return

        bar_height = 12  # Increased height to fit text
        life_percentage = max(0.0, min(1.0, life / max_life))
        life_width = width * life_percentage

        bar_x = px - width / 2

        # Background (black)
        pr.draw_rectangle(int(bar_x), int(py), int(width), bar_height, pr.BLACK)
        # Foreground (green)
        pr.draw_rectangle(int(bar_x), int(py), int(life_width), bar_height, pr.GREEN)

        # Draw life text
        life_text = f"{int(life)} / {int(max_life)}"
        font_size = 10
        text_width = pr.measure_text(life_text, font_size)
        text_x = px - text_width / 2
        text_y = py + (bar_height - font_size) / 2

        # Draw text shadow for readability
        pr.draw_text_ex(
            pr.get_font_default(),
            life_text,
            pr.Vector2(text_x + 1, text_y + 1),
            font_size,
            1,
            pr.BLACK,
        )
        # Draw main text
        pr.draw_text_ex(
            pr.get_font_default(),
            life_text,
            pr.Vector2(text_x, text_y),
            font_size,
            1,
            pr.WHITE,
        )

    def _get_frames_for_state(self, object_layer, direction, mode):
        """
        Selects the appropriate frame list and its corresponding state string from an
        object layer based on direction and mode, with fallbacks.
        """
        frames = object_layer.data.render.frames

        # Handle stateless objects first. They ignore direction and mode.
        if object_layer.data.render.is_stateless:
            # For stateless objects, always use 'none_idle' or 'default_idle' frames,
            # which correspond to the "08" direction code.
            if frames.none_idle:
                return frames.none_idle, "none_idle"
            if frames.default_idle:
                return frames.default_idle, "default_idle"
            return [], None  # Fallback for stateless if no default frames exist

        mode_str = mode.name.lower()
        direction_str = direction.name.lower()

        # 1. Try specific direction and mode (e.g., 'up_walking')
        attr_name = f"{direction_str}_{mode_str}"
        if hasattr(frames, attr_name) and getattr(frames, attr_name):
            return getattr(frames, attr_name), attr_name

        # 2. Fallback to idle for the same direction (e.g., 'up_idle')
        idle_attr_name = f"{direction_str}_idle"
        if hasattr(frames, idle_attr_name) and getattr(frames, idle_attr_name):
            return getattr(frames, idle_attr_name), idle_attr_name

        # 3. Fallback to default idle
        if frames.default_idle:
            return frames.default_idle, "default_idle"

        # 4. Final fallback to 'none' idle
        if frames.none_idle:
            return frames.none_idle, "none_idle"

        return [], None

    def _draw_entity_layers(
        self,
        entity_id,
        pos_vec,
        dims_vec,
        direction,
        mode,
        object_layers_state,
        entity_type=None,
        entity_data=None,
    ):
        cell_size = self.game_state.cell_size if self.game_state.cell_size > 0 else 12.0
        scaled_pos_x = pos_vec.x * cell_size
        scaled_pos_y = pos_vec.y * cell_size
        scaled_dims_w = dims_vec.x * cell_size
        scaled_dims_h = dims_vec.y * cell_size

        dest_rec = pr.Rectangle(
            scaled_pos_x, scaled_pos_y, scaled_dims_w, scaled_dims_h
        )

        if self.game_state.dev_ui and entity_type and entity_data:
            color = pr.BLACK  # fallback
            if entity_type == "self":
                color = self.game_state.colors.get("PLAYER", pr.Color(0, 200, 255, 255))
            elif entity_type == "other":
                color = self.game_state.colors.get(
                    "OTHER_PLAYER", pr.Color(255, 100, 0, 255)
                )
            elif entity_type == "bot":
                behavior = entity_data.get("behavior", "passive")
                if behavior == "hostile":
                    color = self.game_state.colors.get(
                        "ERROR_TEXT", pr.Color(255, 50, 50, 255)
                    )
                else:
                    color = self.game_state.colors.get(
                        "OTHER_PLAYER", pr.Color(100, 200, 100, 255)
                    )
            elif entity_type == "floor":
                color = self.game_state.colors.get(
                    "FLOOR_BACKGROUND", pr.Color(50, 55, 50, 100)
                )
            pr.draw_rectangle_pro(dest_rec, pr.Vector2(0, 0), 0, color)

        if not object_layers_state:
            return

        # Render each active layer, with the loop index acting as a z-index
        for z_index, layer_state in enumerate(object_layers_state):
            if not layer_state.get("active"):
                continue

            item_id = layer_state.get("itemId")
            if not item_id:
                continue

            object_layer = self.obj_layers_mgr.get_or_fetch(item_id)
            if not object_layer:
                continue

            # Manage animation state
            anim_key = (entity_id, item_id)
            now = time.time()

            if anim_key not in self.animation_state_cache:
                self.animation_state_cache[anim_key] = {
                    "frame_index": 0,
                    "last_update_time": now,
                    "last_state_string": None,
                    "last_facing_direction": (
                        direction if direction != Direction.NONE else Direction.DOWN
                    ),
                }
            anim_state = self.animation_state_cache[anim_key]

            # Update last_facing_direction if we have a new direction that is not NONE
            if direction != Direction.NONE:
                anim_state["last_facing_direction"] = direction

            render_direction = direction
            render_mode = mode

            # If the entity is idle and has no direction, use the last facing direction
            # unless that direction was DOWN.
            if direction == Direction.NONE and mode == ObjectLayerMode.IDLE:
                last_facing_direction = anim_state.get(
                    "last_facing_direction", Direction.DOWN
                )
                if last_facing_direction != Direction.DOWN:
                    render_direction = last_facing_direction
                    # render_mode is already IDLE, no change needed.

            frame_list, state_string = self._get_frames_for_state(
                object_layer, render_direction, render_mode
            )
            if not frame_list:
                continue

            # Reset animation if the rendered state string has changed.
            if anim_state.get("last_state_string") != state_string:
                anim_state["frame_index"] = 0
                anim_state["last_update_time"] = now
                anim_state["last_state_string"] = state_string

            num_frames = len(frame_list)
            frame_duration_ms = object_layer.data.render.frame_duration or 100
            # Update frame based on duration
            if (now - anim_state["last_update_time"]) * 1000 >= frame_duration_ms:
                anim_state["frame_index"] = (anim_state["frame_index"] + 1) % num_frames
                anim_state["last_update_time"] = now

            current_frame_number = anim_state["frame_index"]

            # Build texture URI and render
            item = object_layer.data.item
            direction_code = (
                self.obj_layers_mgr.direction_converter.get_code_from_directions(
                    [state_string]
                )
            )
            if not direction_code:
                continue

            uri = self.obj_layers_mgr._build_uri(
                item_type=item.type,
                item_id=item.id,
                direction_code=direction_code,
                frame=current_frame_number,
            )
            texture = self.texture_manager.load_texture_from_url(uri)
            if texture and texture.id > 0:
                source_rec = pr.Rectangle(
                    0, 0, float(texture.width), float(texture.height)
                )
                pr.draw_texture_pro(
                    texture, source_rec, dest_rec, pr.Vector2(0, 0), 0.0, pr.WHITE
                )

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
                dims = p.get("dims", pr.Vector2(1.0, 1.0))
                bottom_y = pos.y + dims.y  # measured in grid cells
                entries.append(("other", bottom_y, player_id, p))
            # bots
            for bot_id, b in self.game_state.bots.items():
                pos = b.get("interp_pos", b.get("pos_server"))
                dims = b.get("dims", pr.Vector2(1.0, 1.0))
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
                        "object_layers": self.game_state.player_object_layers,
                        "life": self.game_state.player_life,
                        "max_life": self.game_state.player_max_life,
                    },
                )
            )

        # sort ascending by bottom_y; smaller Y (higher on map) drawn first
        entries.sort(key=lambda e: e[1])

        # draw in sorted order
        for typ, _, entity_id, data in entries:
            # Extract common entity data
            pos = data.get(
                "interp_pos", data.get("pos", self.game_state.player_pos_interpolated)
            )
            dims = data.get("dims", pr.Vector2(1.0, 1.0))
            direction = data.get("direction", Direction.NONE)
            mode = data.get("mode", ObjectLayerMode.IDLE)
            object_layers = data.get("object_layers", [])
            life = data.get("life", 100.0)
            max_life = data.get("max_life", 100.0)

            # Render the entity's animated layers
            self._draw_entity_layers(
                entity_id, pos, dims, direction, mode, object_layers, typ, data
            )

            # Draw the entity's label on top
            cell_size = (
                self.game_state.cell_size if self.game_state.cell_size > 0 else 12.0
            )
            scaled_pos_x = pos.x * cell_size
            scaled_pos_y = pos.y * cell_size
            scaled_dims_w = dims.x * cell_size
            center_x = scaled_pos_x + scaled_dims_w / 2.0

            # Position label higher to make space for life bar
            label_top_y = scaled_pos_y - 64

            # Position life bar between label and entity
            life_bar_top_y = scaled_pos_y - 22

            self._draw_entity_life_bar(
                center_x, life_bar_top_y, scaled_dims_w, life, max_life
            )
            id_text = entity_id or "you"
            dir_text = (
                direction.name if isinstance(direction, Direction) else str(direction)
            )
            type_text = (
                "Player" if typ in ["self", "other"] else data.get("behavior", "bot")
            )

            self._draw_entity_label(
                center_x,
                label_top_y,
                [str(id_text), str(dir_text), str(type_text)],
                font_size=12,
            )
