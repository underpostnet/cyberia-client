import time
import pyray as pr
from dataclasses import is_dataclass, asdict
from typing import Any, Dict, List, Optional, Tuple
from config import ASSETS_BASE_URL
from src.texture_manager import TextureManager


class Hud:
    """HUD manager for the game UI.

    Notes:
        - Reordering of HUD items was removed to keep indices stable so the
          currently opened view never changes automatically when items are
          toggled on/off.
        - The public API keeps previous method names for compatibility but
          avoids side-effects that change the visible view.
    """

    def __init__(self, client: Any) -> None:
        self.client = client
        self.game_state = client.game_state
        self.texture_manager: TextureManager = client.texture_manager

        # HUD bar state
        self.items: List[Dict[str, Any]] = []  # list of dicts with item data
        self.bar_height: int = 96
        self.bar_padding: int = 12
        self.item_w: int = 80
        self.item_h: int = 72
        self.item_spacing: int = 12
        self.scroll_x: float = 0.0  # negative values scroll left

        # drag state for scroll
        self.dragging: bool = False
        self.drag_start_x: float = 0.0
        self.scroll_start: float = 0.0
        self.drag_moved: bool = False
        self.click_threshold: int = 6  # pixels threshold to consider click vs drag

        # Sub-HUD state (for associated items)
        self.sub_items: List[Dict[str, Any]] = []
        self.sub_bar_height: int = 80
        self.sub_item_w: int = 70
        self.sub_item_h: int = 62
        self.sub_scroll_x: float = 0.0
        self.sub_dragging: bool = False
        self.sub_drag_start_x: float = 0.0
        self.sub_scroll_start: float = 0.0
        self.sub_drag_moved: bool = False

        # view state (store index of selected item or None). Since we removed
        # reorder logic, using an index is stable in the current design.
        self.view_open: bool = False
        self.view_selected: Optional[int] = None  # index of selected item or None
        self.close_w: int = 50
        self.close_h: int = 50

        # When a sub-item is viewed, this stores its index in `sub_items`.
        # `view_selected` still points to the main HUD item.
        self.sub_view_selected_idx: Optional[int] = None

        # HUD alerts
        self.alert_text: str = ""
        self.alert_until: float = 0.0

        # stored rect for view's activate button (so clicks can check bounds)
        self.view_button_rect: Optional[Tuple[float, float, float, float]] = None

        # HUD slide/collapse state and animation
        self.collapsed: bool = False
        # 0.0 = fully visible, 1.0 = fully hidden (off-screen)
        self.slide_progress: float = 0.0
        self.slide_target: float = 0.0
        self.slide_speed: float = 6.0  # progress units per second

        self.animation_cache: Dict[str, Any] = {}
        # toggle button rect
        self.toggle_rect: Optional[Tuple[float, float, float, float]] = None

        # flag to avoid interpreting the toggle click as a HUD item click
        self._ignore_next_hud_click: bool = False
        self._last_toggle_time: float = 0.0
        self._toggle_ignore_timeout: float = 0.18

    # -------------------------- utility helpers --------------------------
    def _stats_to_dict(self, stats: Any) -> Dict[str, int]:
        """Accept Stats dataclass or plain dict, return dict[str,int]."""
        if stats is None:
            return {}
        try:
            if is_dataclass(stats):
                d = asdict(stats)
            elif isinstance(stats, dict):
                d = stats
            else:
                d = {
                    k: getattr(stats, k)
                    for k in [
                        "effect",
                        "resistance",
                        "agility",
                        "range",
                        "intelligence",
                        "utility",
                    ]
                    if hasattr(stats, k)
                }
        except Exception:
            d = {}

        out: Dict[str, int] = {}
        for k, v in d.items():
            try:
                out[k] = int(v)
            except Exception:
                try:
                    out[k] = int(float(v))
                except Exception:
                    out[k] = 0
        return out

    def active_items(self) -> List[Dict[str, Any]]:
        return [it for it in self.items if it["isActive"]]

    def can_activate_item(
        self, item: Dict[str, Any], sum_stats_limit: Optional[int]
    ) -> Tuple[bool, str]:
        # check activable
        if not item.get("isActivable"):
            return False, "Item cannot be activated."

        # You must have at least one skin active.
        item_type_to_activate = item.get("type")
        has_active_skin = any(it["type"] == "skin" for it in self.active_items())
        if not has_active_skin and item_type_to_activate != "skin":
            return False, "You must have at least one skin active."

        item_type_to_activate = item.get("type")
        item_to_be_deactivated = None
        if item_type_to_activate and item_type_to_activate != "unknown":
            for other_item in self.active_items():  # This is a list of dicts
                if other_item.get("type") == item_type_to_activate:
                    item_to_be_deactivated = other_item
                    break

        # check max 4 active
        if not item_to_be_deactivated and len(self.active_items()) >= 4:
            return False, "You cannot activate more than 4 items."

        # check stats sum doesn't exceed limit
        current_sum = self.client.entity_render.get_entity_active_stats_sum(
            self.game_state.player_id
        )
        if item_to_be_deactivated:
            stats_to_remove = self._stats_to_dict(item_to_be_deactivated.get("stats"))
            for v in stats_to_remove.values():
                try:
                    current_sum -= int(v)
                except Exception:
                    pass

        new_sum = current_sum
        stats_map = self._stats_to_dict(item.get("stats"))
        for v in stats_map.values():
            try:
                new_sum += int(v)
            except Exception:
                pass

        if new_sum > (sum_stats_limit or 0):
            return False, f"Activation would exceed stats limit ({sum_stats_limit})."
        return True, ""

    # -------------------------- activation API --------------------------
    def activate_item(self, idx: int, sum_stats_limit: Optional[int]) -> None:
        """Activate item at `idx` if allowed.

        Important: activation no longer reorders the HUD items so the currently
        opened view (by index) does not change automatically.
        """
        if idx < 0 or idx >= len(self.items):
            return
        item_to_activate = self.items[idx]
        if item_to_activate.get("isActive"):
            return  # already active
        ok, reason = self.can_activate_item(item_to_activate, sum_stats_limit)
        if not ok:
            self.show_hud_alert(reason)
            return

        # Deactivate other items of the same type (keep original indices)
        item_type_to_activate = item_to_activate.get("type")
        if item_type_to_activate and item_type_to_activate != "unknown":
            for i, other_item in enumerate(self.items):
                if (
                    other_item["isActive"]
                    and other_item["type"] == item_type_to_activate
                    and i != idx
                ):
                    other_item["isActive"] = False
                    if hasattr(self.client, "send_item_activation"):
                        self.client.send_item_activation(other_item["id"], False)
                    # keep scanning in case multiple exist, but do not reorder

        item_to_activate["isActive"] = True
        if hasattr(self.client, "send_item_activation"):
            self.client.send_item_activation(item_to_activate["id"], True)

        self.show_hud_alert("Item activated.", 1.5)

    def desactivate_item(self, idx: int) -> None:
        """Deprecated spelling kept for compatibility."""
        # alias to the correctly spelled method
        self.deactivate_item(idx)

    def deactivate_item(self, idx: int) -> None:
        if idx < 0 or idx >= len(self.items):
            return
        item = self.items[idx]
        if not item.get("isActive"):
            return

        # Prevent deactivating the last active skin
        if item.get("type") == "skin":
            active_skins = [it for it in self.active_items() if it["type"] == "skin"]
            if len(active_skins) <= 1:
                self.show_hud_alert("Cannot deactivate the last active skin.")
                return

        item["isActive"] = False
        if hasattr(self.client, "send_item_activation"):
            self.client.send_item_activation(item["id"], False)

        self.show_hud_alert("Item deactivated.", 1.0)

    # -------------------------- drawing helpers --------------------------
    def show_hud_alert(self, text: str, duration: float = 2.5) -> None:
        self.alert_text = text
        self.alert_until = time.time() + duration

    def _is_sub_hud_visible(self) -> bool:
        """Checks if the sub-HUD should be rendered."""
        if not self.view_open or self.view_selected is None:
            return False
        if not (0 <= self.view_selected < len(self.items)):
            return False

        main_item_id = self.items[self.view_selected].get("id")
        if not main_item_id:
            return False

        with self.game_state.mutex:
            associated_ids = self.game_state.associated_item_ids.get(main_item_id)

        if not associated_ids:
            return False
        return True

    def _draw_item_animation(
        self, item: Dict[str, Any], dest_rec: pr.Rectangle, is_view: bool = False
    ):
        item_id = item.get("id")
        if not item_id:
            return

        now = time.time()
        anim_key = item_id
        if anim_key not in self.animation_cache:
            self.animation_cache[anim_key] = {
                "frame_index": 0,
                "last_update_time": now,
                "state_string": None,
            }
        anim_state = self.animation_cache[anim_key]

        object_layer = self.client.obj_layers_mgr.get_or_fetch(item_id)
        txt_color = self.game_state.colors.get("UI_TEXT", pr.WHITE)

        if not object_layer:
            pr.draw_rectangle_rec(dest_rec, pr.Color(50, 50, 50, 200))
            pr.draw_text(
                "?",
                int(dest_rec.x + dest_rec.width / 2 - 5),
                int(dest_rec.y + dest_rec.height / 2 - 10),
                20,
                txt_color,
            )
            return

        frames = object_layer.data.render.frames
        frame_list, state_string = [], ""
        if frames.default_idle:
            frame_list, state_string = frames.default_idle, "default_idle"
        elif frames.none_idle:
            frame_list, state_string = frames.none_idle, "none_idle"

        if not frame_list:
            icon_size = 28 if not is_view else 64
            icon = item.get("icon", "?")
            tw = pr.measure_text(icon, icon_size)
            pr.draw_text_ex(
                pr.get_font_default(),
                icon,
                pr.Vector2(
                    dest_rec.x + (dest_rec.width / 2) - (tw / 2),
                    dest_rec.y + (dest_rec.height / 2) - (icon_size / 2),
                ),
                icon_size,
                1,
                txt_color,
            )
            return

        if anim_state.get("state_string") != state_string:
            anim_state["frame_index"] = 0
            anim_state["last_update_time"] = now
            anim_state["state_string"] = state_string

        num_frames = len(frame_list)
        frame_duration_ms = object_layer.data.render.frame_duration or 100
        if (now - anim_state["last_update_time"]) * 1000 >= frame_duration_ms:
            anim_state["frame_index"] = (anim_state["frame_index"] + 1) % num_frames
            anim_state["last_update_time"] = now

        current_frame_number = anim_state["frame_index"]
        item_data = object_layer.data.item
        direction_code = (
            self.client.obj_layers_mgr.direction_converter.get_code_from_directions(
                [state_string]
            )
            or "08"
        )

        uri = self.client.obj_layers_mgr._build_uri(
            item_type=item_data.type,
            item_id=item_data.id,
            direction_code=direction_code,
            frame=current_frame_number,
        )
        texture = self.texture_manager.load_texture_from_url(uri)

        if texture and texture.id > 0:
            tex_w, tex_h = float(texture.width), float(texture.height)
            scale = 1.0
            if tex_h > dest_rec.height:
                scale = dest_rec.height / tex_h
            if tex_w * scale > dest_rec.width:
                scale = dest_rec.width / tex_w

            final_w, final_h = tex_w * scale, tex_h * scale
            final_x = dest_rec.x + (dest_rec.width - final_w) / 2
            final_y = dest_rec.y + (dest_rec.height - final_h) / 2

            final_dest_rec = pr.Rectangle(final_x, final_y, final_w, final_h)
            source_rec = pr.Rectangle(0, 0, tex_w, tex_h)
            pr.draw_texture_pro(
                texture, source_rec, final_dest_rec, pr.Vector2(0, 0), 0.0, pr.WHITE
            )
        else:
            # Fallback if texture fails to load
            icon_size = 28 if not is_view else 64
            icon = item.get("icon", "?")
            tw = pr.measure_text(icon, icon_size)
            pr.draw_text_ex(
                pr.get_font_default(),
                icon,
                pr.Vector2(
                    dest_rec.x + (dest_rec.width / 2) - (tw / 2),
                    dest_rec.y + (dest_rec.height / 2) - (icon_size / 2),
                ),
                icon_size,
                1,
                txt_color,
            )

    def _hud_bar_rect(
        self, screen_width: int, screen_height: int
    ) -> Tuple[int, int, int, int]:
        x = 0
        h = self.bar_height
        base_y = screen_height - h
        hidden_y = screen_height
        y = pr.lerp(base_y, hidden_y, self.slide_progress)
        w = screen_width
        return x, y, w, h

    def _sub_hud_bar_rect(
        self, screen_width: int, screen_height: int
    ) -> Tuple[int, int, int, int]:
        """Calculates the sub-HUD bar's rectangle, positioned above the main HUD."""
        main_hud_x, main_hud_y, main_hud_w, main_hud_h = self._hud_bar_rect(
            screen_width, screen_height
        )
        h = self.sub_bar_height
        y = main_hud_y - h  # Position it directly above the main HUD bar
        x, w = main_hud_x, main_hud_w
        return x, y, w, h

    def draw_hud_item_button(
        self,
        x: float,
        y: float,
        w: float,
        h: float,
        item: Dict[str, Any],
        hovered: bool,
        is_sub_item: bool = False,
    ) -> None:
        bg = pr.Color(36, 36, 36, 220)
        hover_bg = pr.Color(70, 70, 70, 230)
        border = pr.Color(255, 255, 255, 18)
        txt_color = self.game_state.colors.get("UI_TEXT", pr.Color(255, 255, 255, 255))
        is_active = item.get("isActive", False)

        pr.draw_rectangle_pro(
            pr.Rectangle(x, y, w, h), pr.Vector2(0, 0), 0, hover_bg if hovered else bg
        )
        try:
            pr.draw_rectangle_lines(int(x), int(y), int(w), int(h), border)
        except Exception:
            pass

        # Special border for the currently viewed sub-item
        if is_sub_item and item.get("isViewing", False):
            pr.draw_rectangle_lines_ex(
                pr.Rectangle(int(x), int(y), int(w), int(h)),
                3,
                pr.Color(0, 180, 255, 220),  # Blue border for sub-view
            )
        elif is_active:
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

        # --- Draw Icon (Animated) ---
        text_area_h = 34 if not is_sub_item else 0
        icon_area_h = h - text_area_h - 12  # Make more space for text below
        icon_area_w = w - 12  # Available width for the icon
        icon_dest_rec = pr.Rectangle(x + 6, y + 6, icon_area_w, icon_area_h)
        self._draw_item_animation(item, icon_dest_rec, is_view=False)

        if not is_sub_item:
            # --- Draw Name (ID) ---
            name = item.get("name", "")
            name_size = 12
            tw_name = pr.measure_text(name, name_size)
            name_y = y + h - 28
            pr.draw_text_ex(
                pr.get_font_default(),
                name,
                pr.Vector2(x + (w / 2) - (tw_name / 2), name_y),
                name_size,
                1,
                txt_color,
            )

            # --- Draw Type ---
            item_type = item.get("type", "unknown").capitalize()
            type_size = 10
            tw_type = pr.measure_text(item_type, type_size)
            type_x = x + (w / 2) - (tw_type / 2)
            type_y = name_y + name_size + 1  # Position below name

            # Shadow
            pr.draw_text(
                item_type, int(type_x + 1), int(type_y + 1), type_size, pr.BLACK
            )
            # Text
            pr.draw_text(
                item_type,
                int(type_x),
                int(type_y),
                type_size,
                pr.Color(255, 230, 0, 255),
            )

        # --- Draw Quantity (on top of everything else) ---
        quantity = item.get("quantity")
        if quantity is not None and quantity > 1:
            quantity_text = f"x{quantity}"
            quantity_size = 18  # Increased size
            padding = 5
            tw_q = pr.measure_text(quantity_text, quantity_size)

            # Position in top-right corner
            q_x = x + w - tw_q - padding
            q_y = y + padding

            # Draw shadow text
            pr.draw_text(
                quantity_text,
                int(q_x + 1),
                int(q_y + 1),
                quantity_size,
                pr.BLACK,
            )

            # Draw main text in yellow
            pr.draw_text(
                quantity_text,
                int(q_x),
                int(q_y),
                quantity_size,
                pr.Color(255, 230, 0, 255),
            )

    def draw_hud_bar(self, mouse_pos: Any, screen_width: int, screen_height: int):
        x, y, w, h = self._hud_bar_rect(screen_width, screen_height)

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

        if self.slide_progress >= 0.999:
            return None, total_w, inner_w

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
            hovered = (
                mouse_pos.x >= bx
                and mouse_pos.x <= bx + self.item_w
                and mouse_pos.y >= by
                and mouse_pos.y <= by + self.item_h
            )
            if hovered:
                hovered_index = idx
            self.draw_hud_item_button(
                bx, by, self.item_w, self.item_h, item, hovered, is_sub_item=False
            )

        return hovered_index, total_w, inner_w

    def draw_sub_hud_bar(self, mouse_pos: Any, screen_width: int, screen_height: int):
        """Draws the sub-HUD bar for associated items."""
        if not self._is_sub_hud_visible():
            return None, 0, 0

        # Populate sub_items if they are not ready for the current view
        main_item_id = self.items[self.view_selected].get("id")
        with self.game_state.mutex:
            associated_ids = self.game_state.associated_item_ids.get(main_item_id, [])

        # Check if sub_items need rebuilding
        current_sub_item_ids = [item.get("id") for item in self.sub_items]
        if set(current_sub_item_ids) != set(associated_ids):
            new_sub_items = []
            for item_id in associated_ids:
                ol = self.client.obj_layers_mgr.get_or_fetch(item_id)
                if not ol:
                    continue

                # Find if this item is active on the player
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
                        "quantity": 1,  # Associated items are single
                    }
                )
            self.sub_items = new_sub_items

        if not self.sub_items:
            return None, 0, 0

        x, y, w, h = self._sub_hud_bar_rect(screen_width, screen_height)

        inner_x = x + self.bar_padding
        inner_y = y + (h - self.sub_item_h) / 2
        inner_w = w - (self.bar_padding * 2)

        count = len(self.sub_items)
        total_w = count * self.sub_item_w + max(0, (count - 1)) * self.item_spacing

        max_scroll = max(0, total_w - inner_w)
        if self.sub_scroll_x > 0:
            self.sub_scroll_x = 0
        if self.sub_scroll_x < -max_scroll:
            self.sub_scroll_x = -max_scroll

        pr.draw_rectangle_pro(
            pr.Rectangle(int(x), int(y), int(w), int(h)),
            pr.Vector2(0, 0),
            0,
            pr.Color(28, 28, 28, 220),
        )

        offset = inner_x + self.sub_scroll_x
        hovered_index = None
        for idx, item in enumerate(self.sub_items):
            bx = offset + idx * (self.sub_item_w + self.item_spacing)
            by = inner_y
            hovered = (
                mouse_pos.x >= bx
                and mouse_pos.x <= bx + self.sub_item_w
                and mouse_pos.y >= by
                and mouse_pos.y <= by + self.sub_item_h
            )
            if hovered:
                hovered_index = idx

            item["isViewing"] = self.sub_view_selected_idx == idx

            self.draw_hud_item_button(
                bx,
                by,
                self.sub_item_w,
                self.sub_item_h,
                item,
                hovered,
                is_sub_item=True,
            )

        return hovered_index, total_w, inner_w

    def draw_hud_small_button(
        self, x: float, y: float, w: float, h: float, label: str
    ) -> None:
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

    def draw_hud_view(self, screen_width: int, screen_height: int) -> None:
        if self.view_selected is None:
            self.view_button_rect = None
            return
        # guard index validity
        if not (0 <= self.view_selected < len(self.items)):
            self.view_selected = None
            self.view_open = False
            self.view_button_rect = None
            return

        # Determine which item to view: main HUD item or sub-HUD item
        if (
            self.sub_view_selected_idx is not None
            and 0 <= self.sub_view_selected_idx < len(self.sub_items)
        ):
            item = self.sub_items[self.sub_view_selected_idx]
        else:
            # Fallback to main item if sub-item is invalid
            if not (0 <= self.view_selected < len(self.items)):
                self.view_selected = None
                self.view_open = False
                self.view_button_rect = None
                return
            item = self.items[self.view_selected]

        hud_occupied = (1.0 - self.slide_progress) * self.bar_height
        sub_hud_h = self.sub_bar_height if self._is_sub_hud_visible() else 0

        view_x, view_y, view_w, view_h = (
            0,
            0,
            screen_width,
            int(screen_height - hud_occupied - sub_hud_h),
        )

        pr.draw_rectangle_pro(
            pr.Rectangle(view_x, view_y, view_w, view_h),
            pr.Vector2(0, 0),
            0,
            pr.Color(0, 0, 0, 200),
        )

        # --- Layout Panes ---
        margin = 32
        right_pane_w = int(view_w * 0.4)
        left_pane_w = view_w - right_pane_w
        right_pane_x = view_x + left_pane_w

        # --- Right Pane: Animated Preview & Action Button ---
        preview_margin = 20
        preview_bg_x = right_pane_x + preview_margin
        preview_bg_y = view_y + margin
        preview_bg_w = right_pane_w - (preview_margin * 2)
        preview_bg_h = preview_bg_w  # Square preview area

        preview_rect = pr.Rectangle(
            preview_bg_x, preview_bg_y, preview_bg_w, preview_bg_h
        )
        self._draw_item_animation(item, preview_rect, is_view=True)

        # --- Left Pane: Item Details ---
        start_x = view_x + margin
        start_y = view_y + margin
        content_w = left_pane_w - (margin * 2)

        title = item.get("name", "Unknown Item")
        title_size = 36
        pr.draw_text_ex(
            pr.get_font_default(),
            title,
            pr.Vector2(start_x, start_y),
            title_size,
            1,
            pr.WHITE,
        )

        item_type = item.get("type", "unknown").capitalize()
        type_y = start_y + title_size + 12
        pr.draw_text_ex(
            pr.get_font_default(),
            f"Type: {item_type}",
            pr.Vector2(start_x, type_y),
            18,
            1,
            pr.Color(200, 200, 200, 220),
        )

        desc_y = type_y + 32
        desc = item.get("desc", "")
        pr.draw_text_ex(
            pr.get_font_default(),
            desc,
            pr.Vector2(start_x, desc_y),
            16,
            1,
            pr.Color(200, 200, 200, 220),
        )

        stats_y = desc_y + 60
        stats = self._stats_to_dict(item.get("stats"))
        stat_size = 18
        col1_x = start_x
        col2_x = start_x + content_w / 2
        stat_idx = 0
        for k, v in stats.items():
            line = f"{k.capitalize()}: {v}"
            current_x = col1_x if stat_idx % 2 == 0 else col2_x
            current_y = stats_y + (stat_idx // 2) * 24
            pr.draw_text_ex(
                pr.get_font_default(),
                line,
                pr.Vector2(current_x, current_y),
                stat_size,
                1,
                pr.WHITE,
            )
            stat_idx += 1

        info_y = stats_y + ((stat_idx + 1) // 2) * 24 + 20
        current_active_sum = self.client.entity_render.get_entity_active_stats_sum(
            self.game_state.player_id
        )
        item_type_to_activate = item.get("type")
        item_to_be_deactivated = None
        if item_type_to_activate and item_type_to_activate != "unknown":
            for other_item in self.active_items():
                if other_item.get("type") == item_type_to_activate:
                    item_to_be_deactivated = other_item
                    break
        sum_if_activated = current_active_sum
        if item_to_be_deactivated:
            stats_to_remove = self._stats_to_dict(item_to_be_deactivated.get("stats"))
            sum_if_activated -= sum(stats_to_remove.values())
        item_sum = sum(stats.values())
        sum_if_activated += item_sum
        limit = self.game_state.sum_stats_limit or 0

        summary1 = f"Current Active Sum: {current_active_sum}"
        if item_to_be_deactivated:
            stats_to_remove = self._stats_to_dict(item_to_be_deactivated.get("stats"))
            item_sum_display = item_sum - sum(stats_to_remove.values())
            summary2 = (
                f"Change: {item_sum_display:+} | New Sum: {sum_if_activated} / {limit}"
            )
        else:
            summary2 = f"Adds: {item_sum} | New Sum: {sum_if_activated} / {limit}"

        pr.draw_text_ex(
            pr.get_font_default(),
            summary1,
            pr.Vector2(start_x, info_y),
            16,
            1,
            pr.Color(200, 200, 200, 220),
        )
        info_y += 20
        warn_color = (
            self.game_state.colors.get("ERROR_TEXT", pr.Color(255, 80, 80, 255))
            if sum_if_activated > limit
            else self.game_state.colors.get("UI_TEXT", pr.Color(200, 200, 200, 220))
        )
        pr.draw_text_ex(
            pr.get_font_default(),
            summary2,
            pr.Vector2(start_x, info_y),
            16,
            1,
            warn_color,
        )
        info_y += 22

        if not item.get("isActivable"):
            pr.draw_text_ex(
                pr.get_font_default(),
                "This item cannot be activated.",
                pr.Vector2(start_x, info_y),
                16,
                1,
                self.game_state.colors.get("ERROR_TEXT", pr.Color(255, 80, 80, 255)),
            )
            info_y += 20
        elif len(self.active_items()) >= 4 and not item.get("isActive"):
            pr.draw_text_ex(
                pr.get_font_default(),
                "Maximum active items reached (4).",
                pr.Vector2(start_x, info_y),
                16,
                1,
                self.game_state.colors.get("ERROR_TEXT", pr.Color(255, 80, 80, 255)),
            )
            info_y += 20
        elif sum_if_activated > limit and not item.get("isActive"):
            pr.draw_text_ex(
                pr.get_font_default(),
                f"Activation would exceed the limit by {sum_if_activated - limit} points.",
                pr.Vector2(start_x, info_y),
                16,
                1,
                self.game_state.colors.get("ERROR_TEXT", pr.Color(255, 80, 80, 255)),
            )
            info_y += 20
        else:
            remaining_now = limit - current_active_sum
            pr.draw_text_ex(
                pr.get_font_default(),
                f"Points available now: {remaining_now}",
                pr.Vector2(start_x, info_y),
                16,
                1,
                self.game_state.colors.get("UI_TEXT", pr.Color(200, 200, 200, 220)),
            )
            info_y += 20

        # --- Close Button with Hover Effect ---
        close_w, close_h = self.close_w, self.close_h
        close_x, close_y = screen_width - close_w - 12, 12

        mx, my = pr.get_mouse_position().x, pr.get_mouse_position().y
        hovered = (
            mx >= close_x
            and mx <= close_x + close_w
            and my >= close_y
            and my <= close_y + close_h
        )

        hover_scale = 1.15
        final_w = close_w * hover_scale if hovered else close_w
        final_h = close_h * hover_scale if hovered else close_h
        final_x = close_x - (final_w - close_w) / 2
        final_y = close_y - (final_h - close_h) / 2

        close_icon = self.texture_manager.load_ui_icon("close-yellow.png")
        if close_icon and close_icon.id > 0:
            pr.draw_texture_pro(
                close_icon,
                pr.Rectangle(0, 0, close_icon.width, close_icon.height),
                pr.Rectangle(final_x, final_y, final_w, final_h),
                pr.Vector2(0, 0),
                0.0,
                pr.WHITE,
            )
        else:
            # Fallback button has its own hover effect
            self.draw_hud_small_button(close_x, close_y, close_w, close_h, "✕")

        btn_w = preview_bg_w - 20
        btn_h = 45
        btn_x = preview_bg_x + 10
        btn_y = preview_bg_y + preview_bg_h + 20
        if item.get("isActivable"):
            label = "Deactivate" if item.get("isActive") else "Activate"
            ok, _ = (
                self.can_activate_item(item, self.game_state.sum_stats_limit)
                if not item.get("isActive")
                else (True, "")
            )

            btn_bg = pr.Color(60, 60, 60, 230)
            hover_bg = pr.Color(90, 90, 90, 240)
            disabled_bg = pr.Color(40, 40, 40, 160)

            mx, my = pr.get_mouse_position().x, pr.get_mouse_position().y
            hovered = (
                ok
                and mx >= btn_x
                and mx <= btn_x + btn_w
                and my >= btn_y
                and my <= btn_y + btn_h
            )

            final_bg = disabled_bg if not ok else (hover_bg if hovered else btn_bg)

            pr.draw_rectangle_pro(
                pr.Rectangle(int(btn_x), int(btn_y), int(btn_w), int(btn_h)),
                pr.Vector2(0, 0),
                0,
                final_bg,
            )
            pr.draw_rectangle_lines(
                int(btn_x),
                int(btn_y),
                int(btn_w),
                int(btn_h),
                pr.Color(255, 255, 255, 20),
            )

            ts = 20
            tw = pr.measure_text(label, ts)
            icon_name = "unequip.png" if item.get("isActive") else "equip.png"
            icon_texture = self.texture_manager.load_ui_icon(icon_name)

            icon_size = 24
            icon_padding = 8
            total_content_width = tw + icon_padding + icon_size
            start_content_x = btn_x + (btn_w - total_content_width) / 2

            # Draw text
            pr.draw_text_ex(
                pr.get_font_default(),
                label,
                pr.Vector2(start_content_x, btn_y + (btn_h / 2) - (ts / 2)),
                ts,
                1,
                pr.WHITE if ok else pr.GRAY,
            )

            # Draw icon
            if icon_texture and icon_texture.id > 0:
                icon_x = start_content_x + tw + icon_padding
                icon_y = btn_y + (btn_h - icon_size) / 2
                pr.draw_texture_pro(
                    icon_texture,
                    pr.Rectangle(0, 0, icon_texture.width, icon_texture.height),
                    pr.Rectangle(icon_x, icon_y, icon_size, icon_size),
                    pr.Vector2(0, 0),
                    0.0,
                    pr.WHITE if ok else pr.GRAY,
                )

            self.view_button_rect = (btn_x, btn_y, btn_w, btn_h)
        else:
            self.view_button_rect = None

    def draw_hud_alert(self, screen_width: int, screen_height: int) -> None:
        if not self.alert_text or time.time() > self.alert_until:
            return
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

    def draw_hud_toggle(
        self, mouse_pos: Any, screen_width: int, screen_height: int
    ) -> None:
        btn_w = 50
        btn_h = 50
        btn_x = (screen_width / 2) - (btn_w / 2)
        sub_hud_h = self.sub_bar_height if self._is_sub_hud_visible() else 0

        # Calculate the button's position when the HUD is fully visible (slide_progress = 0.0).
        # This should be based on the final resting position of the HUDs, not their animated position.
        base_hud_y = screen_height - self.bar_height
        btn_y_when_visible = base_hud_y - sub_hud_h - btn_h - 12

        # Calculate the button's position when the HUD is fully hidden (slide_progress = 1.0).
        # When hidden, the main HUD is at screen_height. The sub-HUD is just above it.
        # The button should be above the hidden sub-HUD.
        btn_y_when_hidden = screen_height - sub_hud_h - btn_h - 12

        # Interpolate between the two fixed points based on the slide progress.
        btn_y = pr.lerp(btn_y_when_visible, btn_y_when_hidden, self.slide_progress)

        self.toggle_rect = (btn_x, btn_y, btn_w, btn_h)

        pr.draw_rectangle_pro(
            pr.Rectangle(int(btn_x), int(btn_y), int(btn_w), int(btn_h)),
            pr.Vector2(0, 0),
            0,
            pr.Color(0, 0, 0, 0),  # Transparent background
        )

        bx, by, bw, bh = self.toggle_rect
        hovered = (
            mouse_pos.x >= bx
            and mouse_pos.x <= bx + bw
            and mouse_pos.y >= by
            and mouse_pos.y <= by + bh
        )

        icon = self.texture_manager.load_ui_icon("cyberia-white.png")
        if icon and icon.id > 0:
            base_icon_size = btn_w
            hover_scale = 1.15
            icon_size = base_icon_size * hover_scale if hovered else base_icon_size

            icon_x = btn_x + (btn_w - icon_size) / 2
            icon_y = btn_y + (btn_h - icon_size) / 2

            pr.draw_texture_pro(
                icon,
                pr.Rectangle(0, 0, icon.width, icon.height),
                pr.Rectangle(icon_x, icon_y, icon_size, icon_size),
                pr.Vector2(0, 0),
                0.0,
                pr.WHITE,
            )
        else:
            # Fallback to arrows
            arrow = "▲" if self.collapsed else "▼"
            ts = 16
            tw = pr.measure_text(arrow, ts)
            pr.draw_text_ex(
                pr.get_font_default(),
                arrow,
                pr.Vector2(
                    btn_x + (btn_w / 2) - (tw / 2), btn_y + (btn_h / 2) - (ts / 2)
                ),
                ts,
                1,
                self.game_state.colors.get("UI_TEXT", pr.Color(255, 255, 255, 255)),
            )
