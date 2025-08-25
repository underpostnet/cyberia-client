import random
import string
import time
import pyray as pr


class Hud:
    def __init__(self, game_state):
        self.game_state = game_state
        # HUD bar state
        self.items = []  # list of dicts with dummy data
        self.bar_height = 96
        self.bar_padding = 12
        self.item_w = 80
        self.item_h = 72
        self.item_spacing = 12
        self.scroll_x = 0.0  # negative values scroll left
        self.dragging = False
        self.drag_start_x = 0.0
        self.scroll_start = 0.0
        self.drag_moved = False
        self.click_threshold = 6  # pixels threshold to consider a click vs drag

        # view state (antes "modal")
        self.view_open = False
        self.view_selected = None  # index of selected item or None
        self.close_w = 36
        self.close_h = 30

        # HUD alerts
        self.alert_text = ""
        self.alert_until = 0.0

        # stored rect for view's activate button (so click can check bounds)
        self.view_button_rect = None  # (x,y,w,h)

        # HUD slide/collapse state and animation
        self.collapsed = False
        # 0.0 = fully visible, 1.0 = fully hidden (off-screen)
        self.slide_progress = 0.0
        self.slide_target = 0.0
        self.slide_speed = 6.0  # progress units per second

        # position/rect of the small toggle button (updated in draw)
        self.toggle_rect = None

        # flag to avoid interpreting the toggle click as a HUD item click immediately after toggle
        self._ignore_next_hud_click = False
        self._last_toggle_time = 0.0
        self._toggle_ignore_timeout = (
            0.18  # seconds to ignore hud clicks after pressing toggle
        )

    def _generate_dummy_items(self, n):
        base_stats = {
            "effect": 4,
            "resistance": 1,
            "agility": 1,
            "range": 0,
            "intelligence": 6,
            "utility": 8,
        }
        self.items = []
        for i in range(n):
            icon = random.choice(string.ascii_uppercase + string.digits)
            # randomize stats: 0..(base*2) (or small range for zero-base)
            stats = {
                k: random.randint(0, v * 2 if v > 0 else 3)
                for k, v in base_stats.items()
            }
            # isActivable randomly true/false (70% activable)
            is_activable = random.random() < 0.7
            item = {
                "id": f"item_{i}",
                "name": f"Item {i+1}",
                "icon": icon,
                "stats": stats,
                "desc": f"This is a dummy item #{i+1}",
                "isActivable": is_activable,
                "isActive": False,
            }
            self.items.append(item)

    def active_items(self):
        return [it for it in self.items if it.get("isActive")]

    def active_stats_sum(self):
        total = 0
        for it in self.active_items():
            for v in it.get("stats", {}).values():
                try:
                    total += int(v)
                except Exception:
                    pass
        return total

    def can_activate_item(self, item, sum_stats_limit):
        # check activable
        if not item.get("isActivable"):
            return False, "Item cannot be activated."
        # check max 4 active
        if len(self.active_items()) >= 4:
            return False, "You cannot activate more than 4 items."
        # check stats sum doesn't exceed limit
        new_sum = self.active_stats_sum()
        for v in item.get("stats", {}).values():
            try:
                new_sum += int(v)
            except Exception:
                pass
        if new_sum > (sum_stats_limit or 0):
            return (
                False,
                f"Activation would exceed stats limit ({sum_stats_limit}).",
            )
        return True, ""

    def activate_item(self, idx, sum_stats_limit):
        if idx < 0 or idx >= len(self.items):
            return
        item = self.items[idx]
        if item.get("isActive"):
            return  # already active
        ok, reason = self.can_activate_item(item, sum_stats_limit)
        if not ok:
            self.show_hud_alert(reason)
            return
        item["isActive"] = True
        # reorder so active items are first
        self.reorder_hud_items()
        self.show_hud_alert("Item activated.", 1.5)

    def desactivate_item(self, idx):
        if idx < 0 or idx >= len(self.items):
            return
        item = self.items[idx]
        if not item.get("isActive"):
            return
        item["isActive"] = False
        self.reorder_hud_items()
        self.show_hud_alert("Item deactivated.", 1.0)

    def reorder_hud_items(self):
        # Move active items to the front (stable)
        active = [it for it in self.items if it.get("isActive")]
        inactive = [it for it in self.items if not it.get("isActive")]
        self.items = active + inactive
        # clamp active count to 4 just in case (deactivate extras)
        if len(active) > 4:
            # deactivate extras beyond first 4
            for it in active[4:]:
                it["isActive"] = False
            # recompose
            active = [it for it in self.items if it.get("isActive")]
            inactive = [it for it in self.items if not it.get("isActive")]
            self.items = active + inactive

    def show_hud_alert(self, text, duration=2.5):
        self.alert_text = text
        self.alert_until = time.time() + duration

    def _hud_bar_rect(self, screen_width, screen_height):
        # returns (x, y, w, h) in screen coords using slide progress
        x = 0
        h = self.bar_height
        # base Y if fully visible
        base_y = screen_height - h
        # when fully hidden (progress=1), top of HUD will be screen_height (i.e., out of view)
        hidden_y = screen_height
        # linear interpolate between base_y and hidden_y based on progress
        y = pr.lerp(base_y, hidden_y, self.slide_progress)
        w = screen_width
        return x, y, w, h

    def draw_hud_item_button(self, x, y, w, h, item, hovered):
        # simple visual for an item in hud bar using draw_rectangle_pro exclusively
        bg = pr.Color(36, 36, 36, 220)
        hover_bg = pr.Color(70, 70, 70, 230)
        border = pr.Color(255, 255, 255, 18)
        txt_color = self.game_state.colors.get("UI_TEXT", pr.Color(255, 255, 255, 255))

        pr.draw_rectangle_pro(
            pr.Rectangle(x, y, w, h), pr.Vector2(0, 0), 0, hover_bg if hovered else bg
        )
        try:
            pr.draw_rectangle_lines(int(x), int(y), int(w), int(h), border)
        except Exception:
            # draw_rectangle_lines may not exist in some bindings; ignore in that case
            pass

        # if active, draw yellow border overlay (thicker)
        if item.get("isActive"):
            try:
                pr.draw_rectangle_lines_ex(
                    pr.Rectangle(int(x), int(y), int(w), int(h)),
                    3,
                    pr.Color(240, 200, 40, 220),
                )
            except Exception:
                try:
                    pr.draw_rectangle_lines(
                        int(x + 2),
                        int(y + 2),
                        int(w - 4),
                        int(h - 4),
                        pr.Color(240, 200, 40, 220),
                    )
                except Exception:
                    pass

        # icon: big char centered top
        icon_size = 28
        icon = item.get("icon", "?")
        tw = pr.measure_text(icon, icon_size)
        pr.draw_text_ex(
            pr.get_font_default(),
            icon,
            pr.Vector2(x + (w / 2) - (tw / 2), y + 6),
            icon_size,
            1,
            txt_color,
        )

        # name small centered below
        name = item.get("name", "")
        name_size = 12
        tw2 = pr.measure_text(name, name_size)
        pr.draw_text_ex(
            pr.get_font_default(),
            name,
            pr.Vector2(x + (w / 2) - (tw2 / 2), y + h - 20),
            name_size,
            1,
            txt_color,
        )

    def draw_hud_bar(self, mouse_pos, screen_width, screen_height):
        # If HUD is fully hidden (progress near 1.0), do NOT draw the bar.
        # Still compute total widths so scroll clamping logic can use them.
        x, y, w, h = self._hud_bar_rect(screen_width, screen_height)

        inner_x = x + self.bar_padding
        inner_y = y + (h - self.item_h) / 2
        inner_w = w - (self.bar_padding * 2)

        # total width of items
        count = len(self.items)
        total_w = count * self.item_w + (count - 1) * self.item_spacing

        # clamp scroll range
        max_scroll = max(0, total_w - inner_w)
        if self.scroll_x > 0:
            self.scroll_x = 0
        if self.scroll_x < -max_scroll:
            self.scroll_x = -max_scroll

        # If fully hidden, skip drawing the bar entirely (only toggle remains visible)
        if self.slide_progress >= 0.999:
            return None, total_w, inner_w

        # draw background bar using draw_rectangle_pro
        pr.draw_rectangle_pro(
            pr.Rectangle(int(x), int(y), int(w), int(h)),
            pr.Vector2(0, 0),
            0,
            pr.Color(18, 18, 18, 220),
        )
        try:
            pr.draw_rectangle_lines(
                int(x), int(y), int(w), int(h), pr.Color(255, 255, 255, 12)
            )
        except Exception:
            pass

        offset = inner_x + self.scroll_x

        hovered_index = None
        for idx, item in enumerate(self.items):
            bx = offset + idx * (self.item_w + self.item_spacing)
            by = inner_y
            # compute hover in screen coords
            hovered = (
                mouse_pos.x >= bx
                and mouse_pos.x <= bx + self.item_w
                and mouse_pos.y >= by
                and mouse_pos.y <= by + self.item_h
            )
            if hovered:
                hovered_index = idx
            # draw item
            self.draw_hud_item_button(bx, by, self.item_w, self.item_h, item, hovered)

        # Note: toggle button is NOT drawn here anymore (drawn on top via draw_hud_toggle)
        return hovered_index, total_w, inner_w

    def draw_hud_small_button(self, x, y, w, h, label):
        bg = pr.Color(60, 60, 60, 230)
        hover_bg = pr.Color(90, 90, 90, 240)
        border = pr.Color(255, 255, 255, 20)
        txt_color = self.game_state.colors.get("UI_TEXT", pr.Color(255, 255, 255, 255))

        mx = pr.get_mouse_position().x
        my = pr.get_mouse_position().y
        hovered = mx >= x and mx <= x + w and my >= y and my <= y + h

        pr.draw_rectangle_pro(
            pr.Rectangle(int(x), int(y), int(w), int(h)),
            pr.Vector2(0, 0),
            0,
            hover_bg if hovered else bg,
        )
        try:
            pr.draw_rectangle_lines(int(x), int(y), int(w), int(h), border)
        except Exception:
            pass
        ts = 16
        tw = pr.measure_text(label, ts)
        pr.draw_text_ex(
            pr.get_font_default(),
            label,
            pr.Vector2(x + (w / 2) - (tw / 2), y + (h / 2) - (ts / 2)),
            ts,
            1,
            txt_color,
        )

    def draw_hud_view(self, screen_width, screen_height):
        """
        Draw the item view (English text). The view area adapts to the HUD slide state
        so it never overlaps with the HUD bar while the HUD is visible.
        """
        if self.view_selected is None:
            self.view_button_rect = None
            return
        item = self.items[self.view_selected]

        # compute view area (full width, height minus hud_bar_height occupied portion)
        hud_occupied = (1.0 - self.slide_progress) * self.bar_height
        view_x = 0
        view_y = 0
        view_w = screen_width
        view_h = int(screen_height - hud_occupied)

        # background overlay in the view area (semi-transparent black) using draw_rectangle_pro
        pr.draw_rectangle_pro(
            pr.Rectangle(view_x, view_y, view_w, view_h),
            pr.Vector2(0, 0),
            0,
            pr.Color(0, 0, 0, 180),
        )

        # Now draw item info
        margin = 28
        start_x = view_x + margin
        start_y = view_y + margin

        # title (icon + name)
        title = f"{item['icon']}  {item['name']}"
        title_size = 32
        pr.draw_text_ex(
            pr.get_font_default(),
            title,
            pr.Vector2(start_x, start_y),
            title_size,
            1,
            self.game_state.colors.get("UI_TEXT", pr.Color(255, 255, 255, 255)),
        )

        # stats block below title with dynamic totals and warnings
        stats = item.get("stats", {})
        stat_y = start_y + 24 + 28
        stat_size = 18

        # compute sums
        current_active_sum = self.active_stats_sum()
        item_sum = 0
        for v in stats.values():
            try:
                item_sum += int(v)
            except Exception:
                pass
        sum_if_activated = current_active_sum + item_sum
        limit = self.game_state.sum_stats_limit or 0
        remaining_after_if = limit - sum_if_activated
        remaining_now = limit - current_active_sum

        # show each stat line
        for k, v in stats.items():
            line = f"{k.capitalize()}: {v}"
            pr.draw_text_ex(
                pr.get_font_default(),
                line,
                pr.Vector2(start_x, stat_y),
                stat_size,
                1,
                self.game_state.colors.get("UI_TEXT", pr.Color(255, 255, 255, 255)),
            )
            stat_y += 22

        stat_y += 6
        # summary lines
        summary1 = f"Active stats sum: {current_active_sum}"
        summary2 = f"Item adds: {item_sum} -> If activated: {sum_if_activated} / Limit: {limit}"
        pr.draw_text_ex(
            pr.get_font_default(),
            summary1,
            pr.Vector2(start_x, stat_y),
            16,
            1,
            pr.Color(200, 200, 200, 220),
        )
        stat_y += 20
        # color warning if it would exceed
        warn_color = (
            self.game_state.colors.get("ERROR_TEXT", pr.Color(255, 80, 80, 255))
            if sum_if_activated > limit
            else self.game_state.colors.get("UI_TEXT", pr.Color(200, 200, 200, 220))
        )
        pr.draw_text_ex(
            pr.get_font_default(),
            summary2,
            pr.Vector2(start_x, stat_y),
            16,
            1,
            warn_color,
        )
        stat_y += 22

        # extra warnings / info
        if not item.get("isActivable"):
            pr.draw_text_ex(
                pr.get_font_default(),
                "This item cannot be activated.",
                pr.Vector2(start_x, stat_y),
                16,
                1,
                self.game_state.colors.get("ERROR_TEXT", pr.Color(255, 80, 80, 255)),
            )
            stat_y += 20
        elif len(self.active_items()) >= 4 and not item.get("isActive"):
            pr.draw_text_ex(
                pr.get_font_default(),
                "Maximum active items reached (4).",
                pr.Vector2(start_x, stat_y),
                16,
                1,
                self.game_state.colors.get("ERROR_TEXT", pr.Color(255, 80, 80, 255)),
            )
            stat_y += 20
        elif sum_if_activated > limit and not item.get("isActive"):
            pr.draw_text_ex(
                pr.get_font_default(),
                f"Activation would exceed the limit by {sum_if_activated - limit} points.",
                pr.Vector2(start_x, stat_y),
                16,
                1,
                self.game_state.colors.get("ERROR_TEXT", pr.Color(255, 80, 80, 255)),
            )
            stat_y += 20
        else:
            pr.draw_text_ex(
                pr.get_font_default(),
                f"Points available now: {remaining_now}",
                pr.Vector2(start_x, stat_y),
                16,
                1,
                self.game_state.colors.get("UI_TEXT", pr.Color(200, 200, 200, 220)),
            )
            stat_y += 20

        # description below stats
        desc = item.get("desc", "")
        pr.draw_text_ex(
            pr.get_font_default(),
            desc,
            pr.Vector2(start_x, stat_y + 12),
            16,
            1,
            pr.Color(200, 200, 200, 220),
        )

        # close button top-right inside view (use ✕)
        close_x = screen_width - self.close_w - 12
        close_y = 12
        self.draw_hud_small_button(close_x, close_y, self.close_w, self.close_h, "✕")

        # Activation toggle (if activable) - reflect if activation would be allowed
        btn_w = 140
        btn_h = 40
        btn_x = screen_width - margin - btn_w
        btn_y = start_y + 10  # below title area
        if item.get("isActivable"):
            label = "Deactivate" if item.get("isActive") else "Activate"
            # check activation viability and show disabled visual if not allowed
            ok, reason = (
                self.can_activate_item(item, self.game_state.sum_stats_limit)
                if not item.get("isActive")
                else (True, "")
            )
            btn_bg = pr.Color(60, 60, 60, 230) if ok else pr.Color(40, 40, 40, 160)
            pr.draw_rectangle_pro(
                pr.Rectangle(int(btn_x), int(btn_y), int(btn_w), int(btn_h)),
                pr.Vector2(0, 0),
                0,
                btn_bg,
            )
            try:
                pr.draw_rectangle_lines(
                    int(btn_x),
                    int(btn_y),
                    int(btn_w),
                    int(btn_h),
                    pr.Color(255, 255, 255, 20),
                )
            except Exception:
                pass
            ts = 18
            tw = pr.measure_text(label, ts)
            pr.draw_text_ex(
                pr.get_font_default(),
                label,
                pr.Vector2(
                    btn_x + (btn_w / 2) - (tw / 2), btn_y + (btn_h / 2) - (ts / 2)
                ),
                ts,
                1,
                self.game_state.colors.get("UI_TEXT", pr.Color(255, 255, 255, 255)),
            )
            # store button rect for click detection
            self.view_button_rect = (btn_x, btn_y, btn_w, btn_h)
        else:
            self.view_button_rect = None

    def draw_hud_alert(self, screen_width, screen_height):
        if not self.alert_text or time.time() > self.alert_until:
            return
        # draw centered top small alert using draw_rectangle_pro
        w = min(600, int(screen_width * 0.75))
        h = 44
        x = (screen_width - w) / 2
        y = 16
        pr.draw_rectangle_pro(
            pr.Rectangle(int(x), int(y), int(w), int(h)),
            pr.Vector2(0, 0),
            0,
            pr.Color(40, 40, 40, 220),
        )
        try:
            pr.draw_rectangle_lines(
                int(x), int(y), int(w), int(h), pr.Color(255, 200, 40, 220)
            )
        except Exception:
            pass
        ts = 18
        tw = pr.measure_text(self.alert_text, ts)
        pr.draw_text_ex(
            pr.get_font_default(),
            self.alert_text,
            pr.Vector2(x + (w / 2) - (tw / 2), y + (h / 2) - (ts / 2)),
            ts,
            1,
            pr.Color(255, 255, 255, 255),
        )

    def draw_hud_toggle(self, mouse_pos, screen_width, screen_height):
        """
        Draw the toggle button on top of everything (so it overlaps the dev UI if needed).
        Use ▲ (open) / ▼ (close) symbols for clarity.
        The toggle y-position interpolates with hud_slide_progress so it has the same transition.
        """
        btn_w = 72
        btn_h = 22
        btn_x = (screen_width / 2) - (btn_w / 2)

        # compute toggle Y so it follows HUD transition:
        # when HUD visible (progress=0): place the toggle just above the HUD.
        # when HUD hidden (progress=1): place the toggle at bottom edge.
        hud_x, hud_y, hud_w, hud_h = self._hud_bar_rect(screen_width, screen_height)
        btn_y_when_visible = hud_y - btn_h - 8
        btn_y_when_hidden = screen_height - btn_h - 8
        # interpolate based on same progress so toggle moves with HUD
        btn_y = pr.lerp(btn_y_when_visible, btn_y_when_hidden, self.slide_progress)

        # store rect for click detection
        self.toggle_rect = (btn_x, btn_y, btn_w, btn_h)

        # background for toggle
        bg = pr.Color(50, 50, 50, 230)
        pr.draw_rectangle_pro(
            pr.Rectangle(int(btn_x), int(btn_y), int(btn_w), int(btn_h)),
            pr.Vector2(0, 0),
            0,
            bg,
        )
        try:
            pr.draw_rectangle_lines(
                int(btn_x),
                int(btn_y),
                int(btn_w),
                int(btn_h),
                pr.Color(255, 255, 255, 18),
            )
        except Exception:
            pass

        # choose arrow: if hud is collapsed (hidden), show ▲ to indicate open; else ▼ to indicate hide
        arrow = "▲" if self.collapsed else "▼"
        ts = 16
        tw = pr.measure_text(arrow, ts)
        pr.draw_text_ex(
            pr.get_font_default(),
            arrow,
            pr.Vector2(btn_x + (btn_w / 2) - (tw / 2), btn_y + (btn_h / 2) - (ts / 2)),
            ts,
            1,
            self.game_state.colors.get("UI_TEXT", pr.Color(255, 255, 255, 255)),
        )
