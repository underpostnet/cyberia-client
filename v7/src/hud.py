import random
import string


class Hud:
    def __init__(self):
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

    # ---------- dummy items ----------
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

    # ---------- helpers for activation logic ----------
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

    def can_activate_item(self, item):
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
        if new_sum > (self.game_state.sum_stats_limit or 0):
            return (
                False,
                f"Activation would exceed stats limit ({self.game_state.sum_stats_limit}).",
            )
        return True, ""

    def activate_item(self, idx):
        if idx < 0 or idx >= len(self.items):
            return
        item = self.items[idx]
        if item.get("isActive"):
            return  # already active
        ok, reason = self.can_activate_item(item)
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
