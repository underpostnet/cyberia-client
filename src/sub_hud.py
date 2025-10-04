import time
import pyray as pr
from typing import Any, Dict, List, Optional, Tuple

from src.object_layer.object_layer import Stats


class SubHud:
    """Manages the sub-HUD bar for associated items."""

    def __init__(self, client: Any, hud: Any):
        self.client = client
        self.hud = hud  # Reference to the main HUD
        self.game_state = client.game_state

        # Sub-HUD state
        self.items: List[Dict[str, Any]] = []
        self.bar_height: int = 80
        self.item_w: int = 70
        self.item_h: int = 62
        self.item_spacing: int = 12
        self.bar_padding: int = 12
        self.scroll_x: float = 0.0
        self.dragging: bool = False
        self.drag_start_x: float = 0.0
        self.scroll_start: float = 0.0
        self.drag_moved: bool = False
        self.click_threshold: int = 6

    def is_visible(self) -> bool:
        """Checks if the sub-HUD should be rendered."""
        if not self.hud.view_open or self.hud.view_selected is None:
            return False
        if not (0 <= self.hud.view_selected < len(self.hud.items)):
            return False

        main_item_id = self.hud.items[self.hud.view_selected].get("id")
        if not main_item_id:
            return False

        with self.game_state.mutex:
            associated_ids = self.game_state.associated_item_ids.get(main_item_id)

        return bool(associated_ids)

    def get_rect(
        self, screen_width: int, screen_height: int
    ) -> Tuple[int, int, int, int]:
        """Calculates the sub-HUD bar's rectangle, positioned above the main HUD."""
        main_hud_x, main_hud_y, _, _ = self.hud._hud_bar_rect(
            screen_width, screen_height
        )
        h = self.bar_height
        y = main_hud_y - h  # Position it directly above the main HUD bar
        x, w = main_hud_x, screen_width
        return x, y, w, h

    def _populate_items(self):
        """Populates or updates the sub-HUD items based on the current main HUD view."""
        if not self.is_visible():
            self.items = []
            return

        main_item_id = self.hud.items[self.hud.view_selected].get("id")
        with self.game_state.mutex:
            associated_ids = self.game_state.associated_item_ids.get(main_item_id, [])

        current_sub_item_ids = [item.get("id") for item in self.items]
        if set(current_sub_item_ids) == set(associated_ids):
            return  # Items are already up-to-date

        new_sub_items = []
        for item_id in associated_ids:
            ol = self.client.obj_layers_mgr.get_or_fetch(item_id)
            if not ol:
                continue

            is_active = False
            with self.game_state.mutex:
                for layer_state in self.game_state.player.object_layers:
                    if layer_state.itemId == item_id and layer_state.active:
                        is_active = True
                        break

            new_sub_items.append(
                {
                    "id": ol.data.item.id,
                    "name": ol.data.item.id,
                    "icon": ol.data.item.id[:1].upper(),
                    "stats": ol.data.stats or Stats(),
                    "desc": ol.data.item.description,
                    "isActivable": bool(ol.data.item.activable),
                    "isActive": is_active,
                    "type": ol.data.item.type,
                    "quantity": 1,
                }
            )
        self.items = new_sub_items

    def draw(
        self, mouse_pos: Any, screen_width: int, screen_height: int
    ) -> Tuple[Optional[int], int, int]:
        """Draws the sub-HUD bar for associated items."""
        if not self.is_visible():
            return None, 0, 0

        self._populate_items()

        if not self.items:
            return None, 0, 0

        x, y, w, h = self.get_rect(screen_width, screen_height)

        inner_x = x + self.bar_padding
        inner_y = y + (h - self.item_h) / 2
        inner_w = w - (self.bar_padding * 2)

        count = len(self.items)
        total_w = count * self.item_w + max(0, (count - 1)) * self.item_spacing

        max_scroll = max(0, total_w - inner_w)
        if self.scroll_x > 0:
            self.scroll_x = 0
        if self.scroll_x < -max_scroll:
            self.scroll_x = -max_scroll

        pr.draw_rectangle_pro(
            pr.Rectangle(int(x), int(y), int(w), int(h)),
            pr.Vector2(0, 0),
            0,
            pr.Color(28, 28, 28, 220),
        )

        offset = inner_x + self.scroll_x
        hovered_index = None
        for idx, item in enumerate(self.items):
            bx = offset + idx * (self.item_w + self.item_spacing)
            by = inner_y
            hovered = (
                mouse_pos.x >= bx
                and mouse_pos.x <= bx + self.item_w
                and mouse_pos.y >= by
                and mouse_pos.y <= by + self.item_h
            )
            if hovered:
                hovered_index = idx

            item["isViewing"] = self.hud.sub_view_selected_idx == idx

            self.hud.draw_hud_item_button(
                bx, by, self.item_w, self.item_h, item, hovered, is_sub_item=True
            )

        return hovered_index, total_w, inner_w

    def handle_input(
        self,
        mouse_pos: Any,
        mouse_pressed: bool,
        mouse_down: bool,
        mouse_released: bool,
    ) -> bool:
        """Handles all input for the sub-HUD, returns True if input was consumed."""
        if not self.is_visible():
            return False

        shx, shy, shw, shh = self.get_rect(
            self.client.screen_width, self.client.screen_height
        )
        in_sub_hud_area = (
            mouse_pos.x >= shx
            and mouse_pos.x <= shx + shw
            and mouse_pos.y >= shy
            and mouse_pos.y <= shy + shh
        )

        if mouse_pressed and in_sub_hud_area:
            self.dragging = True
            self.drag_start_x = mouse_pos.x
            self.scroll_start = self.scroll_x
            self.drag_moved = False
            return True  # Consume click

        if self.dragging and mouse_down:
            delta = mouse_pos.x - self.drag_start_x
            if abs(delta) > self.click_threshold:
                self.drag_moved = True
            self.scroll_x = self.scroll_start + delta
            return True  # Consume drag

        if self.dragging and mouse_released:
            self.dragging = False
            if not self.drag_moved:
                # This was a click, not a drag. Handle it.
                hovered_index, _, _ = self.draw(
                    mouse_pos, self.client.screen_width, self.client.screen_height
                )
                if hovered_index is not None:
                    with self.game_state.mutex:
                        if self.hud.sub_view_selected_idx == hovered_index:
                            self.hud.sub_view_selected_idx = None
                        else:
                            self.hud.sub_view_selected_idx = hovered_index
            # In either case (drag or click), the input is consumed.
            return True

        return False
