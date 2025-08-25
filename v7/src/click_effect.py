import pyray as pr
import time


class ClickEffect:
    def __init__(self, colors):
        self.list = []
        self.colors = colors

    def add_click_pointer(self, world_pos):
        # world_pos is in world pixels (already converted by get_screen_to_world_2d)
        self.list.append(
            {
                "pos": pr.Vector2(world_pos.x, world_pos.y),
                "t": time.time(),
                "dur": 0.75,
            }
        )

    def update_click_pointers(self):
        now = time.time()
        self.list = [e for e in self.list if (now - e["t"]) < e["dur"]]

    def draw_click_pointers(self):
        now = time.time()
        base_color = self.colors.get("CLICK", pr.Color(255, 255, 255, 220))
        for e in self.list:
            age = now - e["t"]
            dur = e["dur"] if e["dur"] > 0 else 0.0001
            t = max(0.0, min(1.0, age / dur))  # 0..1
            # ease-out for radius
            radius = 6 + 18 * (1 - (1 - t) * (1 - t))
            alpha = int(220 * (1 - t))
            color = pr.Color(base_color.r, base_color.g, base_color.b, alpha)
            cx, cy = int(e["pos"].x), int(e["pos"].y)
            # ring
            pr.draw_circle_lines(cx, cy, int(radius), color)
            # center dot
            pr.draw_circle(cx, cy, 2, color)
            # small crosshair
            arm = int(6 * (1 - t))
            pr.draw_line(cx - arm, cy, cx + arm, cy, color)
            pr.draw_line(cx, cy - arm, cx, cy + arm, color)
