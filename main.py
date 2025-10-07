import pyray as pr
import websocket
from src.network_models import (
    AOIUpdatePayload,
    InitPayload,
    SkillItemIdsPayload,
    VisibleBot,
    VisibleFloor,
    VisibleObject,
)
from src.entity_state import PlayerState, BotState, EntityState
import threading
import json
import time
import math

from config import WS_URL

from src.object_layer.object_layer import Direction, ObjectLayerMode
from src.game_state import GameState
from src.dev_ui import DevUI
from src.click_effect import ClickEffect
from src.hud import Hud
from src.object_layers_management import ObjectLayersManager
from src.texture_manager import TextureManager
from src.floating_text import FloatingTextManager
from src.direction_converter import DirectionConverter
from src.render_core import RenderCore
from src.entity_player_input import EntityPlayerInput
from src.entity_render import EntityRender
from src.entity_player_render import EntityPlayerRender
from src.grid_render import GridRender
from src.entity_bot_render import EntityBotRender
from src.serial import from_dict_generic


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
        self.direction_converter = DirectionConverter()
        self.obj_layers_mgr = ObjectLayersManager(
            texture_manager=self.texture_manager,
            direction_converter=self.direction_converter,
        )
        self.entity_render = EntityRender(
            self.game_state, self.obj_layers_mgr, self.texture_manager
        )
        self.hud = Hud(self)
        self.floating_text_manager = FloatingTextManager(self.game_state)

        # timing
        self._last_frame_time = time.time()

        self.render_core = RenderCore(self)
        self.entity_player_input = EntityPlayerInput(self.game_state)
        self.entity_player_render = EntityPlayerRender(
            self.game_state, self.entity_render
        )

        self.grid_render = GridRender(self.game_state, self.entity_render)

        self.entity_bot_render = EntityBotRender(self.game_state, self.entity_render)
        self.dev_ui = DevUI(self.game_state, self.hud)

    def _calculate_total_coin_quantity(self, object_layers_state: list) -> int:
        total_quantity = 0
        if not object_layers_state:
            return 0
        for layer_state in object_layers_state:
            item_id = layer_state.itemId
            if not item_id:
                continue
            # This might block, but it's what the user asked for.
            # It will be cached after the first time.
            object_layer = self.obj_layers_mgr.get_or_fetch(item_id)
            if object_layer and object_layer.data.item.type == "coin":
                total_quantity += layer_state.quantity
        return total_quantity

    def on_message(self, ws, message):
        with self.game_state.mutex:
            try:
                data = json.loads(message)
                message_type = data.get("type")
                self.game_state.download_size_bytes += len(message)

                if message_type == "init_data":
                    payload_data = data.get("payload", {})
                    payload = from_dict_generic(payload_data, InitPayload)

                    # map/grid
                    self.game_state.grid_w = payload.gridW
                    self.game_state.grid_h = payload.gridH

                    # basic
                    self.game_state.default_obj_width = payload.defaultObjectWidth
                    self.game_state.default_obj_height = payload.defaultObjectHeight
                    self.game_state.cell_size = payload.cellSize
                    self.game_state.fps = payload.fps
                    self.game_state.interpolation_ms = payload.interpolationMs
                    self.game_state.aoi_radius = payload.aoiRadius

                    # colors
                    for name, c in payload.colors.items():
                        self.game_state.colors[name] = pr.Color(c.r, c.g, c.b, c.a)

                    # graphics hints (do NOT call pyray here)
                    self.game_state.camera_smoothing = payload.cameraSmoothing
                    self.game_state.camera_zoom = payload.cameraZoom
                    self.game_state.default_width_screen_factor = (
                        payload.defaultWidthScreenFactor
                    )
                    self.game_state.default_height_screen_factor = (
                        payload.defaultHeightScreenFactor
                    )

                    # sum stats limit per-player (new)
                    self.game_state.sum_stats_limit = payload.sumStatsLimit

                    # devUi toggle
                    self.game_state.dev_ui = payload.devUi

                    # mark and notify main thread
                    self.game_state.init_received = True
                    self.init_event.set()

                elif message_type == "skill_item_ids":
                    payload_data = data.get("payload", {})
                    payload = from_dict_generic(payload_data, SkillItemIdsPayload)
                    if payload.requestedItemId:
                        self.game_state.associated_item_ids[payload.requestedItemId] = (
                            payload.associatedItemIds
                        )

                elif message_type == "aoi_update":
                    payload_data = data.get("payload")
                    if not payload_data:
                        return

                    payload = from_dict_generic(payload_data, AOIUpdatePayload)

                    player_data = payload.player
                    # Update sumStatsLimit if server sends it per-player in AOI (keeps client synced)
                    if player_data:
                        self.game_state.sum_stats_limit = player_data.sumStatsLimit

                    # ---------- Player ----------
                    if player_data:
                        self.game_state.player.pos_prev = (
                            self.game_state.player.interp_pos
                        )

                        old_life = self.game_state.player.life
                        new_life = player_data.life
                        life_diff = new_life - old_life
                        if life_diff != 0:
                            self.floating_text_manager.accumulate_life_change(
                                player_data.id,
                                life_diff,
                                self.game_state.player.interp_pos,
                                self.game_state.player.dims,
                            )

                        # Coin change
                        old_player_object_layers = self.game_state.player.object_layers
                        new_player_object_layers = player_data.objectLayers
                        old_coin_qty = self._calculate_total_coin_quantity(
                            old_player_object_layers
                        )
                        new_coin_qty = self._calculate_total_coin_quantity(
                            new_player_object_layers
                        )
                        coin_diff = new_coin_qty - old_coin_qty
                        if coin_diff != 0:
                            self.floating_text_manager.accumulate_coin_change(
                                player_data.id,
                                coin_diff,
                                self.game_state.player.interp_pos,
                                self.game_state.player.dims,
                            )

                        self.game_state.player_id = player_data.id
                        self.game_state.player.map_id = player_data.MapID
                        self.game_state.player.respawn_in = (
                            player_data.respawnIn
                            if player_data.respawnIn is not None
                            else 0.0
                        )
                        self.game_state.player.life = new_life
                        self.game_state.player.max_life = player_data.maxLife

                        # mode
                        try:
                            self.game_state.player.mode = ObjectLayerMode(
                                player_data.mode
                            )
                        except Exception:
                            self.game_state.player.mode = ObjectLayerMode.IDLE

                        # direction for player
                        try:
                            self.game_state.player.direction = Direction(
                                player_data.direction
                            )
                        except Exception:
                            self.game_state.player.direction = Direction.NONE

                        self.game_state.player.pos_server = pr.Vector2(
                            player_data.Pos.X, player_data.Pos.Y
                        )

                        if self.game_state.player.mode == ObjectLayerMode.TELEPORTING:
                            self.game_state.player.interp_pos = (
                                self.game_state.player.pos_server
                            )
                            self.game_state.player.pos_prev = (
                                self.game_state.player.pos_server
                            )

                        self.game_state.last_update_time = time.time()

                        self.game_state.player.dims = pr.Vector2(
                            player_data.Dims.Width, player_data.Dims.Height
                        )

                        self.game_state.player.path = [
                            pr.Vector2(p.X, p.Y) for p in player_data.path
                        ]

                        self.game_state.player.target_pos = pr.Vector2(
                            player_data.targetPos.X, player_data.targetPos.Y
                        )

                        # HUD items from player's object layers state
                        try:
                            self.game_state.player.object_layers = (
                                player_data.objectLayers
                            )
                            self.hud.items = self.obj_layers_mgr.build_hud_items(
                                player_data.objectLayers
                            )
                        except Exception:
                            pass

                    # ---------- Other players ----------
                    visible_players_data = payload.visiblePlayers
                    # We'll rebuild the other_players dict to match server's visible set,
                    # but interpolate by preserving prev/server when possible.
                    new_other_players: dict[str, EntityState] = {}
                    if visible_players_data:
                        for player_id, p_data in visible_players_data.items():
                            if player_id == self.game_state.player_id:
                                continue
                            try:
                                dir_enum = Direction(p_data.direction)
                            except Exception:
                                dir_enum = Direction.NONE

                            try:
                                mode_enum = ObjectLayerMode(p_data.mode)
                            except Exception:
                                mode_enum = ObjectLayerMode.IDLE

                            object_layers_state = p_data.objectLayers

                            # compute server pos vector
                            server_pos = pr.Vector2(p_data.Pos.X, p_data.Pos.Y)
                            dims_vec = pr.Vector2(p_data.Dims.Width, p_data.Dims.Height)

                            # if we had the player before, carry forward prev/server; else initialize both to server
                            prev_entry = self.game_state.other_players.get(player_id)
                            if prev_entry:
                                old_life = prev_entry.life
                                new_life = p_data.life
                                life_diff = new_life - old_life
                                if life_diff != 0:
                                    self.floating_text_manager.accumulate_life_change(
                                        player_id,
                                        life_diff,
                                        prev_entry.interp_pos,
                                        dims_vec,
                                    )

                                # Coin change
                                old_object_layers = prev_entry.object_layers
                                old_coin_qty = self._calculate_total_coin_quantity(
                                    old_object_layers
                                )
                                new_coin_qty = self._calculate_total_coin_quantity(
                                    object_layers_state
                                )
                                coin_diff = new_coin_qty - old_coin_qty
                                if coin_diff != 0:
                                    self.floating_text_manager.accumulate_coin_change(
                                        player_id,
                                        coin_diff,
                                        prev_entry.interp_pos,
                                        dims_vec,
                                    )

                                pos_prev = prev_entry.pos_server
                                # set new entry preserving previous pos_server as pos_prev for interpolation
                                new_other_players[player_id] = EntityState(
                                    id=player_id,
                                    pos_prev=pos_prev,
                                    pos_server=server_pos,
                                    interp_pos=prev_entry.interp_pos,
                                    dims=dims_vec,
                                    direction=dir_enum,
                                    mode=mode_enum,
                                    object_layers=object_layers_state,
                                    life=new_life,
                                    max_life=p_data.maxLife,
                                    respawn_in=(
                                        p_data.respawnIn
                                        if p_data.respawnIn is not None
                                        else 0.0
                                    ),
                                )
                            else:
                                new_other_players[player_id] = EntityState(
                                    id=player_id,
                                    pos_prev=server_pos,
                                    pos_server=server_pos,
                                    interp_pos=server_pos,
                                    dims=dims_vec,
                                    direction=dir_enum,
                                    mode=mode_enum,
                                    object_layers=object_layers_state,
                                    life=p_data.life,
                                    max_life=p_data.maxLife,
                                    respawn_in=(
                                        p_data.respawnIn
                                        if p_data.respawnIn is not None
                                        else 0.0
                                    ),
                                )
                    # replace other_players atomically
                    self.game_state.other_players = new_other_players

                    # ---------- Visible objects (obstacles, portals, foregrounds, bots) ----------
                    visible_objects_data = payload.visibleGridObjects
                    self.game_state.obstacles = {}
                    self.game_state.portals = {}
                    self.game_state.foregrounds = {}  # reset foregrounds each update
                    self.game_state.floors = {}

                    # reset bots container; we'll rebuild keeping prev positions when possible for interpolation
                    new_bots: dict[str, BotState] = {}

                    if visible_objects_data:
                        for obj_id, obj_data in visible_objects_data.items():
                            obj_type = obj_data.get(
                                "Type"
                            )  # Still need this to dispatch

                            if obj_type == "obstacle":
                                obj = from_dict_generic(obj_data, VisibleObject)
                                self.game_state.obstacles[obj_id] = {
                                    "pos": pr.Vector2(obj.Pos.X, obj.Pos.Y),
                                    "dims": pr.Vector2(obj.Dims.Width, obj.Dims.Height),
                                }
                            elif obj_type == "portal":
                                obj = from_dict_generic(obj_data, VisibleObject)
                                self.game_state.portals[obj_id] = {
                                    "pos": pr.Vector2(obj.Pos.X, obj.Pos.Y),
                                    "dims": pr.Vector2(obj.Dims.Width, obj.Dims.Height),
                                    "label": obj.PortalLabel,
                                }
                            elif obj_type == "foreground":
                                obj = from_dict_generic(obj_data, VisibleObject)
                                self.game_state.foregrounds[obj_id] = {
                                    "pos": pr.Vector2(obj.Pos.X, obj.Pos.Y),
                                    "dims": pr.Vector2(obj.Dims.Width, obj.Dims.Height),
                                }
                            elif obj_type == "floor":
                                obj = from_dict_generic(obj_data, VisibleFloor)
                                self.game_state.floors[obj_id] = {
                                    "pos": pr.Vector2(obj.Pos.X, obj.Pos.Y),
                                    "dims": pr.Vector2(obj.Dims.Width, obj.Dims.Height),
                                    "object_layers": obj.objectLayers,
                                }
                            elif obj_type == "bot":
                                bot_data = from_dict_generic(obj_data, VisibleBot)

                                try:
                                    dir_enum = Direction(bot_data.direction)
                                except Exception:
                                    dir_enum = Direction.NONE

                                try:
                                    mode_enum = ObjectLayerMode(bot_data.mode)
                                except Exception:
                                    mode_enum = ObjectLayerMode.IDLE

                                server_pos = pr.Vector2(bot_data.Pos.X, bot_data.Pos.Y)
                                dims_vec = pr.Vector2(
                                    bot_data.Dims.Width, bot_data.Dims.Height
                                )

                                prev_bot = self.game_state.bots.get(obj_id)
                                if prev_bot:
                                    old_life = prev_bot.life
                                    new_life = bot_data.life
                                    life_diff = new_life - old_life
                                    if life_diff != 0 and bot_data.behavior != "bullet":
                                        self.floating_text_manager.accumulate_life_change(
                                            obj_id,
                                            life_diff,
                                            prev_bot.interp_pos,
                                            dims_vec,
                                        )

                                    # Coin change
                                    old_object_layers = prev_bot.object_layers
                                    old_coin_qty = self._calculate_total_coin_quantity(
                                        old_object_layers
                                    )
                                    new_coin_qty = self._calculate_total_coin_quantity(
                                        bot_data.objectLayers
                                    )
                                    coin_diff = new_coin_qty - old_coin_qty
                                    if coin_diff != 0:
                                        self.floating_text_manager.accumulate_coin_change(
                                            obj_id,
                                            coin_diff,
                                            prev_bot.interp_pos,
                                            dims_vec,
                                        )

                                    pos_prev = prev_bot.pos_server
                                    new_bots[obj_id] = BotState(
                                        id=obj_id,
                                        pos_prev=pos_prev,
                                        pos_server=server_pos,
                                        interp_pos=prev_bot.interp_pos,
                                        dims=dims_vec,
                                        behavior=bot_data.behavior,
                                        direction=dir_enum,
                                        mode=mode_enum,
                                        object_layers=bot_data.objectLayers,
                                        life=new_life,
                                        max_life=bot_data.maxLife,
                                        respawn_in=(
                                            bot_data.respawnIn
                                            if bot_data.respawnIn is not None
                                            else 0.0
                                        ),
                                    )
                                else:
                                    new_bots[obj_id] = BotState(
                                        id=obj_id,
                                        pos_prev=server_pos,
                                        pos_server=server_pos,
                                        interp_pos=server_pos,
                                        dims=dims_vec,
                                        behavior=bot_data.behavior,
                                        direction=dir_enum,
                                        mode=mode_enum,
                                        object_layers=bot_data.objectLayers,
                                        life=bot_data.life,
                                        max_life=bot_data.maxLife,
                                        respawn_in=(
                                            bot_data.respawnIn
                                            if bot_data.respawnIn is not None
                                            else 0.0
                                        ),
                                    )

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

    def send_get_item_ids(self, item_id: str):
        """Request associated item IDs for a given item."""
        if not self.ws:
            return

        message = {
            "type": "get_items_ids",
            "payload": {"itemId": item_id},
        }
        try:
            self.ws.send(json.dumps(message))
        except Exception as e:
            print(f"Error sending get_items_ids: {e}")

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
                        with self.game_state.mutex:
                            # If clicking the same item that's already open, close the view.
                            if (
                                self.hud.view_open
                                and self.hud.view_selected == hovered_index
                                and self.hud.sub_view_selected_idx is None
                            ):
                                self.hud.view_open = False
                                self.hud.view_selected = None
                            else:
                                # Open/view item, or switch to it.
                                self.hud.view_open = True
                                self.hud.view_selected = hovered_index
                                self.hud.sub_view_selected_idx = None  # Reset sub-view
                                # Request associated items for the new view
                                item_id = self.hud.items[hovered_index].get("id")
                                if item_id:
                                    self.send_get_item_ids(item_id)
                        consumed_click = True
                # finish dragging
                self.hud.dragging = False
                self.hud.drag_moved = False
                # after release, clear the temporary ignore (if set) to allow normal clicks next frames
                if self.hud._ignore_next_hud_click:
                    self.hud._ignore_next_hud_click = False

            # Sub-HUD input handling (drag, scroll, click)
            if not consumed_click:
                consumed_click = self.hud.sub_hud.handle_input(
                    mouse_pos, mouse_pressed, mouse_down, mouse_released
                )

            # If view open, check close button pressed (top-right inside view area)
            if (
                self.hud.view_open
                and self.hud.view_selected is not None
                and mouse_pressed
            ):
                # Calculate view area, accounting for both HUDs
                hud_occupied = (1.0 - self.hud.slide_progress) * self.hud.bar_height
                sub_hud_h = (
                    self.hud.sub_hud.bar_height if self.hud.sub_hud.is_visible() else 0
                )
                view_y_max = self.screen_height - hud_occupied - sub_hud_h

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
                            self.hud.sub_view_selected_idx = None
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
                                    # Determine if we are activating a main or sub item
                                    if self.hud.sub_view_selected_idx is not None:
                                        # This part is complex: activating a sub-item
                                        # would require knowing its index in the main `items` list.
                                        # For now, we'll just show an alert.
                                        # A full implementation would need to map sub-item IDs
                                        # back to their main list index if they exist there.
                                        self.hud.show_hud_alert(
                                            "Sub-item activation not implemented."
                                        )
                                    else:
                                        sel = self.hud.view_selected
                                        if sel is not None and 0 <= sel < len(
                                            self.hud.items
                                        ):
                                            if self.hud.items[sel]["isActive"]:
                                                self.hud.desactivate_item(sel)
                                            else:
                                                self.hud.activate_item(
                                                    sel, self.game_state.sum_stats_limit
                                                )
                                consumed_click = True
                        # clicks anywhere inside view area (except hud_bar) should not pass to world
                        if not consumed_click:
                            consumed_click = True

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
                                        if self.hud.items[sel]["isActive"]:
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
                        self.game_state.player.interp_pos.x
                        + self.game_state.player.dims.x / 2.0
                    )
                    * cell_size,
                    (
                        self.game_state.player.interp_pos.y
                        + self.game_state.player.dims.y / 2.0
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
                                        if self.hud.items[sel]["isActive"]:
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
                        self.game_state.player.interp_pos.x
                        + self.game_state.player.dims.x / 2.0
                    )
                    * cell_size,
                    (
                        self.game_state.player.interp_pos.y
                        + self.game_state.player.dims.y / 2.0
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
