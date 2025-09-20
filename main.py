import pyray as pr
import websocket
import threading
import json
import time
import math

from config import WS_URL

from src.object_layer import Direction, ObjectLayerMode
from src.game_state import GameState
from src.dev_ui import DevUI
from src.click_effect import ClickEffect
from src.hud import Hud
from src.object_layers_management import ObjectLayersManager
from src.texture_manager import TextureManager
from src.floating_text import FloatingTextManager
from src.direction_converter import DirectionConverter
from src.util import Util
from src.render_core import RenderCore
from src.entity_player_input import EntityPlayerInput
from src.entity_render import EntityRender
from src.entity_player_render import EntityPlayerRender
from src.grid_render import GridRender
from src.entity_bot_render import EntityBotRender


class NetworkClient:
    def __init__(self):
        self.game_state = GameState()
        self.ws = None
        self.ws_thread = None
        self.is_running = True

        # runtime window (computed after init_data)
        self.screen_width = 1280
        self.screen_height = 800

        # client-only click effects (gfx_click_pointer)
        # Each item: {"pos": Vector2 (world coords, pixels), "t": created_time, "dur": seconds}
        self.click_effect = ClickEffect(self.game_state.colors)

        # event to signal main thread to initialize graphics
        self.init_event = threading.Event()

        self.texture_manager = TextureManager()
        self.hud = Hud(self)
        self.direction_converter = DirectionConverter()
        # object layers/cache manager for HUD items
        self.obj_layers_mgr = ObjectLayersManager(
            texture_manager=self.texture_manager,
            direction_converter=self.direction_converter,
        )
        self.floating_text_manager = FloatingTextManager(self.game_state)

        # timing
        self._last_frame_time = time.time()
        self.util = Util()
        self.render_core = RenderCore(self)
        self.entity_render = EntityRender(
            self.game_state, self.obj_layers_mgr, self.texture_manager
        )

        self.entity_player_input = EntityPlayerInput(self.game_state)
        self.entity_player_render = EntityPlayerRender(
            self.game_state, self.entity_render
        )

        self.grid_render = GridRender(self.game_state, self.entity_render)

        self.entity_bot_render = EntityBotRender(self.game_state, self.entity_render)
        self.dev_ui = DevUI(self.game_state, self.hud)

    def on_message(self, ws, message):
        with self.game_state.mutex:
            try:
                data = json.loads(message)
                message_type = data.get("type")
                self.game_state.download_size_bytes += len(message)

                if message_type == "init_data":
                    payload = data.get("payload", {})

                    # map/grid
                    self.game_state.grid_w = int(payload.get("gridW", 100))
                    self.game_state.grid_h = int(payload.get("gridH", 100))

                    # basic
                    self.game_state.default_obj_width = float(
                        payload.get("defaultObjectWidth", 1.0)
                    )
                    self.game_state.default_obj_height = float(
                        payload.get("defaultObjectHeight", 1.0)
                    )
                    self.game_state.cell_size = float(payload.get("cellSize", 12.0))
                    self.game_state.fps = int(payload.get("fps", 60))
                    self.game_state.interpolation_ms = int(
                        payload.get("interpolationMs", 200)
                    )
                    self.game_state.aoi_radius = float(payload.get("aoiRadius", 15.0))

                    # colors
                    colors_payload = payload.get("colors", {})
                    for name, cdict in colors_payload.items():
                        self.game_state.colors[name] = self.util.color_from_payload(
                            cdict
                        )

                    # graphics hints (do NOT call pyray here)
                    try:
                        self.game_state.camera_smoothing = float(
                            payload.get("cameraSmoothing", 0.15)
                        )
                    except Exception:
                        self.game_state.camera_smoothing = 0.15
                    try:
                        self.game_state.camera_zoom = float(
                            payload.get("cameraZoom", 1.0)
                        )
                    except Exception:
                        self.game_state.camera_zoom = 1.0
                    try:
                        self.game_state.default_width_screen_factor = float(
                            payload.get("defaultWidthScreenFactor", 0.5)
                        )
                    except Exception:
                        self.game_state.default_width_screen_factor = 0.5
                    try:
                        self.game_state.default_height_screen_factor = float(
                            payload.get("defaultHeightScreenFactor", 0.5)
                        )
                    except Exception:
                        self.game_state.default_height_screen_factor = 0.5

                    # sum stats limit per-player (new)
                    try:
                        self.game_state.sum_stats_limit = int(
                            payload.get("sumStatsLimit", 9999)
                        )
                    except Exception:
                        self.game_state.sum_stats_limit = 9999

                    # devUi toggle
                    try:
                        self.game_state.dev_ui = bool(payload.get("devUi", False))
                    except Exception:
                        self.game_state.dev_ui = False

                    # mark and notify main thread
                    self.game_state.init_received = True
                    self.init_event.set()

                elif message_type == "aoi_update":
                    payload = data.get("payload")
                    if not payload:
                        return

                    player_data = payload.get("player")
                    # Update sumStatsLimit if server sends it per-player in AOI (keeps client synced)
                    if player_data and "sumStatsLimit" in player_data:
                        try:
                            self.game_state.sum_stats_limit = int(
                                player_data.get(
                                    "sumStatsLimit", self.game_state.sum_stats_limit
                                )
                            )
                        except Exception:
                            pass

                    # ---------- Player ----------
                    if player_data:
                        self.game_state.player_pos_prev = (
                            self.game_state.player_pos_interpolated
                        )
                        old_life = self.game_state.player_life
                        new_life = float(player_data.get("life", 100.0))
                        life_diff = new_life - old_life
                        if life_diff != 0:
                            self.floating_text_manager.accumulate_life_change(
                                player_data.get("id"),
                                life_diff,
                                self.game_state.player_pos_interpolated,
                                self.game_state.player_dims,
                            )

                        self.game_state.player_id = player_data.get("id")
                        self.game_state.player_map_id = int(player_data.get("MapID", 0))
                        self.game_state.player_respawn_in = float(
                            player_data.get("respawnIn", 0.0)
                        )
                        self.game_state.player_life = new_life

                        self.game_state.player_max_life = float(
                            player_data.get("maxLife", 100.0)
                        )

                        # mode
                        mode_val = player_data.get("mode", 0)
                        try:
                            mode_int = int(mode_val)
                        except (TypeError, ValueError):
                            mode_int = 0
                        try:
                            self.game_state.player_mode = ObjectLayerMode(mode_int)
                        except Exception:
                            self.game_state.player_mode = ObjectLayerMode.IDLE

                        # direction
                        direction_val = player_data.get("direction", 8)
                        try:
                            dir_int = int(direction_val)
                        except (TypeError, ValueError):
                            dir_int = 8
                        try:
                            self.game_state.player_direction = Direction(dir_int)
                        except Exception:
                            self.game_state.player_direction = Direction.NONE

                        pos = player_data.get("Pos", {})
                        self.game_state.player_pos_server = pr.Vector2(
                            pos.get("X", 0.0), pos.get("Y", 0.0)
                        )

                        if self.game_state.player_mode == ObjectLayerMode.TELEPORTING:
                            self.game_state.player_pos_interpolated = (
                                self.game_state.player_pos_server
                            )
                            self.game_state.player_pos_prev = (
                                self.game_state.player_pos_server
                            )

                        self.game_state.last_update_time = time.time()

                        dims = player_data.get("Dims", {})
                        self.game_state.player_dims = pr.Vector2(
                            dims.get("Width", self.game_state.default_obj_width),
                            dims.get("Height", self.game_state.default_obj_height),
                        )

                        path_data = player_data.get("path")
                        if path_data is not None:
                            self.game_state.path = [
                                pr.Vector2(p.get("X"), p.get("Y")) for p in path_data
                            ]
                        else:
                            self.game_state.path = []

                        target_pos_data = player_data.get("targetPos")
                        if target_pos_data:
                            self.game_state.target_pos = pr.Vector2(
                                target_pos_data.get("X"), target_pos_data.get("Y")
                            )
                        else:
                            self.game_state.target_pos = pr.Vector2(-1, -1)

                        # HUD items from player's object layers state
                        try:
                            object_layers_state = player_data.get("objectLayers")
                            if isinstance(object_layers_state, list):
                                self.game_state.player_object_layers = (
                                    object_layers_state
                                )
                                self.hud.items = self.obj_layers_mgr.build_hud_items(
                                    object_layers_state
                                )

                        except Exception:
                            pass

                    # ---------- Other players ----------
                    visible_players_data = payload.get("visiblePlayers")
                    # We'll rebuild the other_players dict to match server's visible set,
                    # but interpolate by preserving prev/server when possible.
                    new_other_players = {}
                    if visible_players_data:
                        for player_id, p_data in visible_players_data.items():
                            if player_id == self.game_state.player_id:
                                continue
                            pos = p_data.get("Pos", {})
                            dims = p_data.get("Dims", {})
                            direction_val = p_data.get("direction", 8)
                            try:
                                dir_int = int(direction_val)
                            except (TypeError, ValueError):
                                dir_int = 8
                            try:
                                dir_enum = Direction(dir_int)
                            except Exception:
                                dir_enum = Direction.NONE

                            mode_val = p_data.get("mode", 0)
                            try:
                                mode_int = int(mode_val)
                            except (TypeError, ValueError):
                                mode_int = 0
                            try:
                                mode_enum = ObjectLayerMode(mode_int)
                            except Exception:
                                mode_enum = ObjectLayerMode.IDLE

                            object_layers_state = p_data.get("objectLayers", [])

                            # compute server pos vector
                            server_pos = pr.Vector2(pos.get("X"), pos.get("Y"))
                            dims_vec = pr.Vector2(
                                dims.get("Width", self.game_state.default_obj_width),
                                dims.get("Height", self.game_state.default_obj_height),
                            )

                            # if we had the player before, carry forward prev/server; else initialize both to server
                            prev_entry = self.game_state.other_players.get(player_id)
                            if prev_entry:
                                old_life = prev_entry.get("life", 100.0)
                                new_life = float(p_data.get("life", 100.0))
                                life_diff = new_life - old_life
                                if life_diff != 0:
                                    self.floating_text_manager.accumulate_life_change(
                                        player_id,
                                        life_diff,
                                        prev_entry.get("interp_pos", server_pos),
                                        dims_vec,
                                    )

                                pos_prev = prev_entry.get("pos_server", server_pos)
                                # set new entry preserving previous pos_server as pos_prev for interpolation
                                new_other_players[player_id] = {
                                    "pos_prev": pos_prev,
                                    "pos_server": server_pos,
                                    "interp_pos": prev_entry.get(
                                        "interp_pos", server_pos
                                    ),
                                    "dims": dims_vec,
                                    "direction": dir_enum,
                                    "mode": mode_enum,
                                    "last_update": time.time(),
                                    "object_layers": object_layers_state,
                                    "life": new_life,
                                    "max_life": float(p_data.get("maxLife", 100.0)),
                                    "respawnIn": float(p_data.get("respawnIn", 0.0)),
                                }
                            else:
                                new_other_players[player_id] = {
                                    "pos_prev": server_pos,
                                    "pos_server": server_pos,
                                    "interp_pos": server_pos,
                                    "dims": dims_vec,
                                    "direction": dir_enum,
                                    "mode": mode_enum,
                                    "last_update": time.time(),
                                    "object_layers": object_layers_state,
                                    "life": float(p_data.get("life", 100.0)),
                                    "max_life": float(p_data.get("maxLife", 100.0)),
                                    "respawnIn": float(p_data.get("respawnIn", 0.0)),
                                }
                    # replace other_players atomically
                    self.game_state.other_players = new_other_players

                    # ---------- Visible objects (obstacles, portals, foregrounds, bots) ----------
                    visible_objects_data = payload.get("visibleGridObjects")
                    self.game_state.obstacles = {}
                    self.game_state.portals = {}
                    self.game_state.foregrounds = {}  # reset foregrounds each update
                    self.game_state.floors = {}

                    # reset bots container; we'll rebuild keeping prev positions when possible for interpolation
                    new_bots = {}

                    if visible_objects_data:
                        for obj_id, obj_data in visible_objects_data.items():
                            obj_type = obj_data.get("Type")
                            pos = obj_data.get("Pos", {})
                            dims = obj_data.get("Dims", {})
                            if obj_type == "obstacle":
                                self.game_state.obstacles[obj_id] = {
                                    "pos": pr.Vector2(pos.get("X"), pos.get("Y")),
                                    "dims": pr.Vector2(
                                        dims.get("Width"), dims.get("Height")
                                    ),
                                }
                            elif obj_type == "portal":
                                self.game_state.portals[obj_id] = {
                                    "pos": pr.Vector2(pos.get("X"), pos.get("Y")),
                                    "dims": pr.Vector2(
                                        dims.get("Width"), dims.get("Height")
                                    ),
                                    "label": obj_data.get("PortalLabel"),
                                }
                            elif obj_type == "foreground":
                                self.game_state.foregrounds[obj_id] = {
                                    "pos": pr.Vector2(pos.get("X"), pos.get("Y")),
                                    "dims": pr.Vector2(
                                        dims.get("Width"), dims.get("Height")
                                    ),
                                }
                            elif obj_type == "floor":
                                self.game_state.floors[obj_id] = {
                                    "pos": pr.Vector2(pos.get("X"), pos.get("Y")),
                                    "dims": pr.Vector2(
                                        dims.get("Width"), dims.get("Height")
                                    ),
                                    "object_layers": obj_data.get("objectLayers", []),
                                }
                            elif obj_type == "bot":
                                # PARSE bot fields (new)
                                behavior = obj_data.get("behavior", "passive")
                                object_layers_state = obj_data.get("objectLayers", [])
                                direction_val = obj_data.get("direction", 8)
                                try:
                                    dir_int = int(direction_val)
                                except (TypeError, ValueError):
                                    dir_int = 8
                                try:
                                    dir_enum = Direction(dir_int)
                                except Exception:
                                    dir_enum = Direction.NONE

                                mode_val = obj_data.get("mode", 0)
                                try:
                                    mode_int = int(mode_val)
                                except (TypeError, ValueError):
                                    mode_int = 0
                                try:
                                    mode_enum = ObjectLayerMode(mode_int)
                                except Exception:
                                    mode_enum = ObjectLayerMode.IDLE

                                # safe defaults for dims
                                w = dims.get("Width", self.game_state.default_obj_width)
                                h = dims.get(
                                    "Height", self.game_state.default_obj_height
                                )
                                server_pos = pr.Vector2(
                                    pos.get("X", 0.0), pos.get("Y", 0.0)
                                )
                                dims_vec = pr.Vector2(w, h)

                                prev_bot = self.game_state.bots.get(obj_id)
                                if prev_bot:
                                    old_life = prev_bot.get("life", 100.0)
                                    new_life = float(obj_data.get("life", 100.0))
                                    life_diff = new_life - old_life
                                    if life_diff != 0 and behavior != "bullet":
                                        self.floating_text_manager.accumulate_life_change(
                                            obj_id,
                                            life_diff,
                                            prev_bot.get("interp_pos", server_pos),
                                            dims_vec,
                                        )

                                    pos_prev = prev_bot.get("pos_server", server_pos)
                                    new_bots[obj_id] = {
                                        "pos_prev": pos_prev,
                                        "pos_server": server_pos,
                                        "interp_pos": prev_bot.get(
                                            "interp_pos", server_pos
                                        ),
                                        "dims": dims_vec,
                                        "behavior": behavior,
                                        "direction": dir_enum,
                                        "mode": mode_enum,
                                        "last_update": time.time(),
                                        "object_layers": object_layers_state,
                                        "life": new_life,
                                        "max_life": float(
                                            obj_data.get("maxLife", 100.0)
                                        ),
                                        "respawnIn": float(
                                            obj_data.get("respawnIn", 0.0)
                                        ),
                                    }
                                else:
                                    new_bots[obj_id] = {
                                        "pos_prev": server_pos,
                                        "pos_server": server_pos,
                                        "interp_pos": server_pos,
                                        "dims": dims_vec,
                                        "behavior": behavior,
                                        "direction": dir_enum,
                                        "mode": mode_enum,
                                        "last_update": time.time(),
                                        "object_layers": object_layers_state,
                                        "life": float(obj_data.get("life", 100.0)),
                                        "max_life": float(
                                            obj_data.get("maxLife", 100.0)
                                        ),
                                        "respawnIn": float(
                                            obj_data.get("respawnIn", 0.0)
                                        ),
                                    }

                    # atomically replace bots
                    self.game_state.bots = new_bots

            except json.JSONDecodeError as e:
                with self.game_state.mutex:
                    self.game_state.last_error_message = f"JSON Decode Error: {e}"
                    self.game_state.error_display_time = time.time()
                print(f"JSON Decode Error: {e}")
            except Exception as e:
                with self.game_state.mutex:
                    self.game_state.last_error_message = f"Error: {e}"
                    self.game_state.error_display_time = time.time()
                print(f"Error: {e}")

    def send_item_activation(self, item_id: str, is_active: bool):
        """Send item activation state to the server"""
        if not self.ws:
            return

        message = {
            "type": "item_activation",
            "payload": {"itemId": item_id, "active": is_active},
        }
        try:
            self.ws.send(json.dumps(message))
            self.game_state.upload_size_bytes += len(json.dumps(message))
        except Exception as e:
            print(f"Error sending item activation: {e}")

    def on_error(self, ws, error):
        with self.game_state.mutex:
            self.game_state.last_error_message = f"WebSocket Error: {error}"
            self.game_state.error_display_time = time.time()
        print(f"### WebSocket Error ###: {error}")

    def on_close(self, ws, close_status_code, close_msg):
        with self.game_state.mutex:
            self.game_state.last_error_message = (
                f"WebSocket Closed: {close_status_code}, {close_msg}"
            )
            self.game_state.error_display_time = time.time()
        print("### WebSocket Closed ###")
        self.ws = None

    def on_open(self, ws):
        print("WebSocket Opened. Sending join request...")
        try:
            join_message = {"type": "join_request", "payload": {}}
            self.ws.send(json.dumps(join_message))
        except Exception as e:
            print(f"Error sending join request: {e}")

    def run_websocket_thread(self):
        self.ws = websocket.WebSocketApp(
            WS_URL,
            on_open=self.on_open,
            on_message=self.on_message,
            on_error=self.on_error,
            on_close=self.on_close,
        )
        self.ws.run_forever(reconnect=5)

    def run_game_loop(self):
        # use server fps if available, else fallback 60
        target_fps = self.game_state.fps if self.game_state.fps > 0 else 60

        last_download_check_time = time.time()
        while not pr.window_should_close() and self.is_running:
            now = time.time()
            dt = (
                now - self._last_frame_time
                if self._last_frame_time
                else (1.0 / target_fps)
            )
            self._last_frame_time = now

            # Process any pending texture caching requests from the network thread
            self.obj_layers_mgr.process_texture_caching_queue()

            # read mouse input early (for UI hit testing)
            mouse_pos = pr.get_mouse_position()
            mouse_pressed = pr.is_mouse_button_pressed(
                pr.MOUSE_LEFT_BUTTON
            )  # down this frame
            mouse_down = pr.is_mouse_button_down(pr.MOUSE_LEFT_BUTTON)  # is held
            mouse_released = pr.is_mouse_button_released(
                pr.MOUSE_LEFT_BUTTON
            )  # released this frame

            consumed_click = False

            # clear temporary ignore if timeout passed
            if (
                self.hud._ignore_next_hud_click
                and (now - self.hud._last_toggle_time) > self.hud._toggle_ignore_timeout
            ):
                self.hud._ignore_next_hud_click = False

            # animate HUD slide progress toward target
            if abs(self.hud.slide_progress - self.hud.slide_target) > 0.0001:
                direction = (
                    1.0 if self.hud.slide_target > self.hud.slide_progress else -1.0
                )
                self.hud.slide_progress += direction * self.hud.slide_speed * dt
                # clamp
                if self.hud.slide_progress < 0.0:
                    self.hud.slide_progress = 0.0
                if self.hud.slide_progress > 1.0:
                    self.hud.slide_progress = 1.0
                # update collapsed state when animation reaches ends
                self.hud.collapsed = True if self.hud.slide_progress >= 0.999 else False

            # HUD BAR DRAG/CLICK handling:
            hx, hy, hw, hh = self.hud._hud_bar_rect(
                self.screen_width, self.screen_height
            )
            in_hud_area = (
                mouse_pos.x >= hx
                and mouse_pos.x <= hx + hw
                and mouse_pos.y >= hy
                and mouse_pos.y <= hy + hh
            )

            # start drag if pressed inside hud area (only when hud visible) and not ignoring due to toggle
            if (
                mouse_pressed
                and in_hud_area
                and self.hud.slide_progress < 0.999
                and not self.hud._ignore_next_hud_click
            ):
                self.hud.dragging = True
                self.hud.drag_start_x = mouse_pos.x
                self.hud.scroll_start = self.hud.scroll_x
                self.hud.drag_moved = False
                consumed_click = (
                    True  # pressing on hud consumes click so it won't pass to world
                )

            # if dragging, update scroll while mouse held
            if self.hud.dragging and mouse_down:
                delta = mouse_pos.x - self.hud.drag_start_x
                if abs(delta) > self.hud.click_threshold:
                    self.hud.drag_moved = True
                self.hud.scroll_x = self.hud.scroll_start + delta
                consumed_click = True

            # on release finalize: if it was a short click (no movement) treat as click on item
            if self.hud.dragging and mouse_released:
                # compute hovered item and layout by calling draw_hud_bar (cheap)
                hovered_index, total_w, inner_w = self.hud.draw_hud_bar(
                    mouse_pos, self.screen_width, self.screen_height
                )
                max_scroll = max(0, total_w - inner_w)
                # clamp
                if self.hud.scroll_x > 0:
                    self.hud.scroll_x = 0
                if self.hud.scroll_x < -max_scroll:
                    self.hud.scroll_x = -max_scroll

                delta = mouse_pos.x - self.hud.drag_start_x
                # Only open view if we are NOT ignoring due to a toggle press that just happened
                if (
                    (not self.hud.drag_moved)
                    and (abs(delta) <= self.hud.click_threshold)
                    and (not self.hud._ignore_next_hud_click)
                ):
                    hovered_index, _, _ = self.hud.draw_hud_bar(
                        mouse_pos, self.screen_width, self.screen_height
                    )
                    if hovered_index is not None:
                        # open/view item (or switch view if already open)
                        with self.game_state.mutex:
                            self.hud.view_open = True
                            self.hud.view_selected = hovered_index
                        consumed_click = True
                # finish dragging
                self.hud.dragging = False
                self.hud.drag_moved = False
                # after release, clear the temporary ignore (if set) to allow normal clicks next frames
                if self.hud._ignore_next_hud_click:
                    self.hud._ignore_next_hud_click = False

            # HUD toggle button click (handle is separate from hud area)
            if mouse_pressed and self.hud.toggle_rect:
                bx, by, bw, bh = self.hud.toggle_rect
                if (
                    mouse_pos.x >= bx
                    and mouse_pos.x <= bx + bw
                    and mouse_pos.y >= by
                    and mouse_pos.y <= by + bh
                ):
                    # toggle collapsed state: set animation target
                    self.hud.collapsed = not self.hud.collapsed
                    self.hud.slide_target = 1.0 if self.hud.collapsed else 0.0
                    # set ignore flag for a brief window so the same mouse press/release doesn't open an item
                    self.hud._ignore_next_hud_click = True
                    self.hud._last_toggle_time = time.time()
                    consumed_click = True
                    # ensure we don't accidentally start a drag in same frame
                    self.hud.dragging = False
                    self.hud.drag_moved = False

            # If view open, check close button pressed (top-right inside view area)
            if (
                self.hud.view_open
                and self.hud.view_selected is not None
                and mouse_pressed
            ):
                # only check close if click is inside view area (above the hud_bar)
                hud_occupied = (1.0 - self.hud.slide_progress) * self.hud.bar_height
                view_y_max = self.screen_height - hud_occupied
                if mouse_pos.y <= view_y_max:
                    # check close button
                    close_x = self.screen_width - self.hud.close_w - 12
                    close_y = 12
                    if (
                        mouse_pos.x >= close_x
                        and mouse_pos.x <= close_x + self.hud.close_w
                        and mouse_pos.y >= close_y
                        and mouse_pos.y <= close_y + self.hud.close_h
                    ):
                        with self.game_state.mutex:
                            self.hud.view_open = False
                            self.hud.view_selected = None
                        consumed_click = True
                    else:
                        # check activate/deactivate button (if present)
                        if self.hud.view_button_rect:
                            bx, by, bw, bh = self.hud.view_button_rect
                            if (
                                mouse_pos.x >= bx
                                and mouse_pos.x <= bx + bw
                                and mouse_pos.y >= by
                                and mouse_pos.y <= by + bh
                            ):
                                # toggle activation
                                with self.game_state.mutex:
                                    sel = self.hud.view_selected
                                    if sel is not None and 0 <= sel < len(
                                        self.hud.items
                                    ):
                                        if self.hud.items[sel].get("isActive"):
                                            self.hud.desactivate_item(sel)
                                        else:
                                            self.hud.activate_item(
                                                sel, self.game_state.sum_stats_limit
                                            )
                                consumed_click = True
                        # clicks anywhere inside view area (except hud_bar) should not pass to world
                        if not consumed_click:
                            consumed_click = True

            # If not consumed by UI, handle world click for movement
            if not consumed_click and mouse_pressed:
                try:
                    world_pos = pr.get_screen_to_world_2d(
                        mouse_pos, self.game_state.camera
                    )
                    target_x = world_pos.x / (
                        self.game_state.cell_size
                        if self.game_state.cell_size > 0
                        else 12.0
                    )
                    target_y = world_pos.y / (
                        self.game_state.cell_size
                        if self.game_state.cell_size > 0
                        else 12.0
                    )
                    target_x = math.floor(target_x)
                    target_y = math.floor(target_y)
                    self.entity_player_input.send_player_action(
                        self.ws, target_x, target_y
                    )
                    # client-side click pointer effect
                    self.click_effect.add_click_pointer(world_pos)
                except Exception:
                    pass

            # bandwidth
            current_time = time.time()
            if current_time - last_download_check_time >= 1:
                with self.game_state.mutex:
                    self.dev_ui.download_kbps = (
                        self.game_state.download_size_bytes / 1024
                    ) * 8
                    self.dev_ui.upload_kbps = (
                        self.game_state.upload_size_bytes / 1024
                    ) * 8
                    self.game_state.download_size_bytes = 0
                    self.game_state.upload_size_bytes = 0
                last_download_check_time = current_time

            # interpolation (player + entities)
            self.entity_player_render.interpolate_player_position()
            self.entity_render.interpolate_entities_positions()

            # camera smoothing: compute desired world target and smooth camera.target to it
            try:
                cell_size = (
                    self.game_state.cell_size if self.game_state.cell_size > 0 else 12.0
                )
                # Use player's center so camera keeps player centered regardless of its dimensions
                desired_center = pr.Vector2(
                    (
                        self.game_state.player_pos_interpolated.x
                        + self.game_state.player_dims.x / 2.0
                    )
                    * cell_size,
                    (
                        self.game_state.player_pos_interpolated.y
                        + self.game_state.player_dims.y / 2.0
                    )
                    * cell_size,
                )
                if self.game_state.camera is None:
                    # safety: create default camera if missing
                    offset = pr.Vector2(self.screen_width / 2, self.screen_height / 2)
                    self.game_state.camera = pr.Camera2D(
                        offset,
                        desired_center,
                        0.0,
                        float(
                            self.game_state.camera_zoom
                            if self.game_state.camera_zoom is not None
                            else 1.0
                        ),
                    )
                current_target = self.game_state.camera.target
                smooth = (
                    float(self.game_state.camera_smoothing)
                    if self.game_state.camera_smoothing is not None
                    else 0.15
                )
                # clamp smoothing to sensible range
                if smooth < 0:
                    smooth = 0
                if smooth > 1:
                    smooth = 1
                new_tx = pr.lerp(current_target.x, desired_center.x, smooth)
                new_ty = pr.lerp(current_target.y, desired_center.y, smooth)
                # apply
                try:
                    self.game_state.camera.target = pr.Vector2(new_tx, new_ty)
                    # ensure offset remains centered each frame (good practice)
                    try:
                        self.game_state.camera.offset = pr.Vector2(
                            self.screen_width / 2, self.screen_height / 2
                        )
                    except Exception:
                        pass
                except Exception:
                    # in some bindings camera fields are attributes directly accessible; attempt attribute set
                    try:
                        setattr(
                            self.game_state.camera, "target", pr.Vector2(new_tx, new_ty)
                        )
                        try:
                            setattr(
                                self.game_state.camera,
                                "offset",
                                pr.Vector2(
                                    self.screen_width / 2, self.screen_height / 2
                                ),
                            )
                        except Exception:
                            pass
                    except Exception:
                        pass
            except Exception:
                pass

            # update client-side effects
            self.click_effect.update_click_pointers()
            self.floating_text_manager.update(dt)

            # draw
            self.render_core.draw_game()

        print("Closing WebSocket...")
        if self.ws:
            self.ws.close()
        self.is_running = False
        pr.close_window()


if __name__ == "__main__":
    client = NetworkClient()
    print("Starting WebSocket client thread...")
    client.ws_thread = threading.Thread(target=client.run_websocket_thread, daemon=True)
    client.ws_thread.start()

    # Wait until init_data arrives
    print("Waiting for init_data from server before initializing graphics...")
    client.init_event.wait()
    if not client.game_state.init_received:
        print("init_event triggered but init_received false — aborting.")
    else:
        print("init_data received — initializing graphics on main thread.")
        client.render_core.initialize_graphics()

        # Start the render/game loop (blocks)
        client.run_game_loop()
