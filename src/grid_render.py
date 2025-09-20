import pyray as pr
from src.object_layer.object_layer import Direction, ObjectLayerMode


class GridRender:
    def __init__(self, game_state, entity_render):
        self.game_state = game_state
        self.entity_render = entity_render

    def draw_grid_background(self):
        grid_bg_color = self.game_state.colors.get("GRID_BACKGROUND", None)
        if grid_bg_color:
            grid_w = self.game_state.grid_w if self.game_state.grid_w > 0 else 100
            grid_h = self.game_state.grid_h if self.game_state.grid_h > 0 else 100
            cell_size = (
                self.game_state.cell_size if self.game_state.cell_size > 0 else 12.0
            )
            map_w, map_h = grid_w * cell_size, grid_h * cell_size
            pr.draw_rectangle(0, 0, int(map_w), int(map_h), grid_bg_color)

    def draw_grid_lines(self):
        if not self.game_state.dev_ui:
            return

        grid_w = self.game_state.grid_w if self.game_state.grid_w > 0 else 100
        grid_h = self.game_state.grid_h if self.game_state.grid_h > 0 else 100
        cell_size = self.game_state.cell_size if self.game_state.cell_size > 0 else 12.0
        map_w, map_h = grid_w * cell_size, grid_h * cell_size

        # default to a dark gray boundary so map area doesn't look white
        color_boundary = self.game_state.colors.get(
            "MAP_BOUNDARY", pr.Color(60, 60, 60, 255)
        )

        # draw only the rectangle outline (no fill)
        # prefer draw_rectangle_lines_ex if available, else use draw_rectangle_lines
        try:
            pr.draw_rectangle_lines_ex(
                pr.Rectangle(0, 0, map_w, map_h), 1, color_boundary
            )
        except Exception:
            pr.draw_rectangle_lines(0, 0, int(map_w), int(map_h), color_boundary)

        # fade color for grid lines
        fade_col = self.game_state.colors.get("MAP_GRID", None)
        if fade_col is None:
            # use fade(color_boundary, 0.2) if available, else create a faded color manually
            if hasattr(pr, "fade"):
                fade_col = pr.fade(color_boundary, 0.2)
            else:
                # manual fade (reduce alpha)
                fade_col = pr.Color(
                    color_boundary.r,
                    color_boundary.g,
                    color_boundary.b,
                    int(color_boundary.a * 0.2),
                )

        for i in range(grid_w + 1):
            x = i * cell_size
            try:
                pr.draw_line_ex(pr.Vector2(x, 0), pr.Vector2(x, map_h), 1, fade_col)
            except Exception:
                pr.draw_line(int(x), 0, int(x), int(map_h), fade_col)

        for j in range(grid_h + 1):
            y = j * cell_size
            try:
                pr.draw_line_ex(pr.Vector2(0, y), pr.Vector2(map_w, y), 1, fade_col)
            except Exception:
                pr.draw_line(0, int(y), int(map_w), int(y), fade_col)

    def draw_grid_floors(self):
        cell_size = self.game_state.cell_size if self.game_state.cell_size > 0 else 12.0
        with self.game_state.mutex:
            for obj_id, obj_data in self.game_state.floors.items():
                pos = obj_data["pos"]
                dims = obj_data["dims"]
                object_layers = obj_data.get("object_layers", [])

                if object_layers:
                    self.entity_render._draw_entity_layers(
                        entity_id=obj_id,
                        pos_vec=pos,
                        dims_vec=dims,
                        direction=Direction.NONE,
                        mode=ObjectLayerMode.IDLE,
                        object_layers_state=object_layers,
                        entity_type="floor",
                        entity_data=obj_data,
                    )
                else:
                    # Fallback to drawing a simple rectangle
                    pr.draw_rectangle_pro(
                        pr.Rectangle(
                            pos.x * cell_size,
                            pos.y * cell_size,
                            dims.x * cell_size,
                            dims.y * cell_size,
                        ),
                        pr.Vector2(0, 0),
                        0,
                        self.game_state.colors.get(
                            "FLOOR_BACKGROUND", pr.Color(50, 55, 50, 255)
                        ),
                    )

    def draw_grid_objects(self):
        cell_size = self.game_state.cell_size if self.game_state.cell_size > 0 else 12.0
        with self.game_state.mutex:
            for obj_id, obj_data in self.game_state.obstacles.items():
                pos = obj_data["pos"]
                dims = obj_data["dims"]
                pr.draw_rectangle_pro(
                    pr.Rectangle(
                        pos.x * cell_size,
                        pos.y * cell_size,
                        dims.x * cell_size,
                        dims.y * cell_size,
                    ),
                    pr.Vector2(0, 0),
                    0,
                    self.game_state.colors.get(
                        "OBSTACLE", pr.Color(100, 100, 100, 255)
                    ),
                )

            for portal_id, portal_data in self.game_state.portals.items():
                pos = portal_data["pos"]
                dims = portal_data["dims"]
                label = portal_data.get("label", "")
                pr.draw_rectangle_pro(
                    pr.Rectangle(
                        pos.x * cell_size,
                        pos.y * cell_size,
                        dims.x * cell_size,
                        dims.y * cell_size,
                    ),
                    pr.Vector2(0, 0),
                    0,
                    self.game_state.colors.get("PORTAL", pr.Color(180, 50, 255, 180)),
                )
                # draw label centered (draw_text_ex)
                label_pos = pr.Vector2(
                    (pos.x + dims.x / 2) * cell_size, (pos.y + dims.y / 2) * cell_size
                )
                pr.draw_text_ex(
                    pr.get_font_default(),
                    label,
                    pr.Vector2(
                        label_pos.x - pr.measure_text(label, 10) / 2, label_pos.y - 6
                    ),
                    10,
                    1,
                    self.game_state.colors.get(
                        "PORTAL_LABEL", pr.Color(240, 240, 240, 255)
                    ),
                )
