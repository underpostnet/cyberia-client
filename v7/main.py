import pyray as pr
import websocket
import threading
import json
import time
import math
import ctypes


from src.object_layer import Direction, ObjectLayerMode
from src.game_state import GameState
from src.ws_client import WSClient
from src.dev_ui import DevUI
from src.click_effect import ClickEffect
from src.hud import Hud
from config import WS_URL


class NetworkClient:
    def __init__(self):
        self.game_state = GameState()
        self.ws_client = WSClient()
        self.dev_ui = DevUI()

        # runtime window (computed after init_data)
        self.screen_width = 1280
        self.screen_height = 800

        # client-only click effects (gfx_click_pointer)
        # Each item: {"pos": Vector2 (world coords, pixels), "t": created_time, "dur": seconds}
        self.click_effect = ClickEffect(self.game_state.colors)

        # event to signal main thread to initialize graphics
        self.init_event = threading.Event()

        self.hud = Hud()
        # prepare dummy items now
        self.hud._generate_dummy_items(10)

        # timing
        self._last_frame_time = time.time()

    # ---------- network handlers ----------
    def color_from_payload(self, cdict):
        try:
            r = int(cdict.get("r", 255))
            g = int(cdict.get("g", 255))
            b = int(cdict.get("b", 255))
            a = int(cdict.get("a", 255))
            return pr.Color(r, g, b, a)
        except Exception:
            return pr.Color(255, 255, 255, 255)

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
                        self.game_state.colors[name] = self.color_from_payload(cdict)

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
                        self.game_state.player_id = player_data.get("id")
                        self.game_state.player_map_id = int(player_data.get("MapID", 0))

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

                            # compute server pos vector
                            server_pos = pr.Vector2(pos.get("X"), pos.get("Y"))
                            dims_vec = pr.Vector2(
                                dims.get("Width", self.game_state.default_obj_width),
                                dims.get("Height", self.game_state.default_obj_height),
                            )

                            # if we had the player before, carry forward prev/server; else initialize both to server
                            prev_entry = self.game_state.other_players.get(player_id)
                            if prev_entry:
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
                                }
                    # replace other_players atomically
                    self.game_state.other_players = new_other_players

                    # ---------- Visible objects (obstacles, portals, foregrounds, bots) ----------
                    visible_objects_data = payload.get("visibleGridObjects")
                    self.game_state.obstacles = {}
                    self.game_state.portals = {}
                    self.game_state.foregrounds = {}  # reset foregrounds each update

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
                            elif obj_type == "bot":
                                # PARSE bot fields (new)
                                behavior = obj_data.get("behavior", "passive")
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
        self.ws_client.ws = None

    def on_open(self, ws):
        print("WebSocket Opened. Sending join request...")
        try:
            join_message = {"type": "join_request", "payload": {}}
            self.ws_client.ws.send(json.dumps(join_message))
        except Exception as e:
            print(f"Error sending join request: {e}")

    def run_websocket_thread(self):
        self.ws_client.ws = websocket.WebSocketApp(
            WS_URL,
            on_open=self.on_open,
            on_message=self.on_message,
            on_error=self.on_error,
            on_close=self.on_close,
        )
        self.ws_client.ws.run_forever(reconnect=5)

    # ---------- start / initialization ----------
    def start(self):
        print("Starting WebSocket client thread...")
        self.ws_client.ws_thread = threading.Thread(
            target=self.run_websocket_thread, daemon=True
        )
        self.ws_client.ws_thread.start()

        # Wait until init_data arrives
        print("Waiting for init_data from server before initializing graphics...")
        self.init_event.wait()
        if not self.game_state.init_received:
            print("init_event triggered but init_received false — aborting.")
            return
        print("init_data received — initializing graphics on main thread.")
        self.initialize_graphics()

        # Start the render/game loop (blocks)
        self.run_game_loop()

    # ---------- monitor detection helpers ----------
    def detect_monitor_size(self):
        # Try several strategies in order; return (w, h)
        # 1) pyray monitor functions
        try:
            if (
                hasattr(pr, "get_monitor_count")
                and hasattr(pr, "get_monitor_width")
                and hasattr(pr, "get_monitor_height")
            ):
                # try primary monitor 0
                try:
                    cnt = pr.get_monitor_count()
                except Exception:
                    cnt = 1
                try:
                    mw = pr.get_monitor_width(0)
                    mh = pr.get_monitor_height(0)
                    if mw and mh:
                        return int(mw), int(mh)
                except Exception:
                    pass
            # older bindings: get_screen_width / get_screen_height
            if hasattr(pr, "get_screen_width") and hasattr(pr, "get_screen_height"):
                try:
                    mw = pr.get_screen_width()
                    mh = pr.get_screen_height()
                    if mw and mh:
                        return int(mw), int(mh)
                except Exception:
                    pass
        except Exception:
            pass

        # 2) tkinter fallback (works on many platforms)
        try:
            import tkinter as tk

            root = tk.Tk()
            root.withdraw()
            mw = root.winfo_screenwidth()
            mh = root.winfo_screenheight()
            root.destroy()
            if mw and mh:
                return int(mw), int(mh)
        except Exception:
            pass

        # 3) Windows ctypes fallback
        try:
            user32 = ctypes.windll.user32
            # Try to set DPI aware (best-effort)
            try:
                user32.SetProcessDPIAware()
            except Exception:
                pass
            mw = user32.GetSystemMetrics(0)
            mh = user32.GetSystemMetrics(1)
            if mw and mh:
                return int(mw), int(mh)
        except Exception:
            pass

        # 4) final fallback to defaults
        return 1280, 800

    def initialize_graphics(self):
        # Called on main thread after init_data arrived
        monitor_w, monitor_h = self.detect_monitor_size()

        w_factor = (
            self.game_state.default_width_screen_factor
            if self.game_state.default_width_screen_factor is not None
            else 0.5
        )
        h_factor = (
            self.game_state.default_height_screen_factor
            if self.game_state.default_height_screen_factor is not None
            else 0.5
        )

        # Compute window size
        sw = max(640, int(monitor_w * float(w_factor)))
        sh = max(480, int(monitor_h * float(h_factor)))

        # ensure we don't exceed monitor
        sw = min(sw, monitor_w)
        sh = min(sh, monitor_h)

        self.screen_width = sw
        self.screen_height = sh

        print(
            f"Monitor detected: {monitor_w}x{monitor_h} -> Window: {self.screen_width}x{self.screen_height}"
        )

        # Initialize pyray window and fps
        pr.set_config_flags(pr.ConfigFlags.FLAG_VSYNC_HINT)
        pr.init_window(self.screen_width, self.screen_height, "MMO Client")

        target_fps = self.game_state.fps if self.game_state.fps > 0 else 60
        pr.set_target_fps(target_fps)

        # Camera creation:
        try:
            cam_zoom = (
                float(self.game_state.camera_zoom)
                if self.game_state.camera_zoom is not None
                else 1.0
            )
        except Exception:
            cam_zoom = 1.0

        # initial target = player center position * cell_size (may be zero)
        try:
            player_center_x = (
                self.game_state.player_pos_interpolated.x
                + self.game_state.player_dims.x / 2.0
            ) * (self.game_state.cell_size if self.game_state.cell_size > 0 else 12.0)
            player_center_y = (
                self.game_state.player_pos_interpolated.y
                + self.game_state.player_dims.y / 2.0
            ) * (self.game_state.cell_size if self.game_state.cell_size > 0 else 12.0)
            initial_target = pr.Vector2(player_center_x, player_center_y)
        except Exception:
            initial_target = pr.Vector2(0, 0)

        offset = pr.Vector2(self.screen_width / 2, self.screen_height / 2)
        # create camera with offset first (many python bindings expect (offset, target, rot, zoom))
        try:
            self.game_state.camera = pr.Camera2D(offset, initial_target, 0.0, cam_zoom)
        except Exception:
            # fallback in case signature differs; try swapping args
            try:
                self.game_state.camera = pr.Camera2D(
                    initial_target, offset, 0.0, cam_zoom
                )
            except Exception:
                # final fallback: construct with zeros then assign attributes if possible
                self.game_state.camera = pr.Camera2D(
                    pr.Vector2(0, 0), pr.Vector2(0, 0), 0.0, cam_zoom
                )
                try:
                    self.game_state.camera.offset = offset
                    self.game_state.camera.target = initial_target
                except Exception:
                    pass

        # store camera offset for our use (we will keep camera.offset centered)
        try:
            # ensure offset is set correctly
            self.game_state.camera.offset = pr.Vector2(
                self.screen_width / 2, self.screen_height / 2
            )
        except Exception:
            pass

    # ---------- input / send ----------
    def send_player_action(self, target_x, target_y):
        if (
            self.ws_client.ws
            and self.ws_client.ws.sock
            and self.ws_client.ws.sock.connected
        ):
            try:
                action_message = {
                    "type": "player_action",
                    "payload": {"targetX": target_x, "targetY": target_y},
                }
                self.ws_client.ws.send(json.dumps(action_message))
                self.game_state.upload_size_bytes += len(json.dumps(action_message))
            except websocket.WebSocketConnectionClosedException:
                print("Cannot send message, connection is closed.")
            except Exception as e:
                print(f"Error sending message: {e}")

    # ---------- interpolation & drawing ----------
    def interpolate_player_position(self):
        with self.game_state.mutex:
            interp_ms = (
                self.game_state.interpolation_ms
                if self.game_state.interpolation_ms > 0
                else 200
            )
            time_since_update = time.time() - self.game_state.last_update_time
            interp_factor = min(1.0, time_since_update / (interp_ms / 1000.0))

            current_x = pr.lerp(
                self.game_state.player_pos_prev.x,
                self.game_state.player_pos_server.x,
                interp_factor,
            )
            current_y = pr.lerp(
                self.game_state.player_pos_prev.y,
                self.game_state.player_pos_server.y,
                interp_factor,
            )
            self.game_state.player_pos_interpolated = pr.Vector2(current_x, current_y)

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

    def draw_grid_lines(self):
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

    # --- helpers to draw single entities (used by sorted renderer) ---
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

    def _draw_player_at(
        self,
        pos_vec,
        dims_vec,
        is_self=False,
        direction=Direction.NONE,
        mode=ObjectLayerMode.IDLE,
        entity_id=None,
    ):
        cell_size = self.game_state.cell_size if self.game_state.cell_size > 0 else 12.0
        scaled_pos_x = pos_vec.x * cell_size
        scaled_pos_y = pos_vec.y * cell_size
        scaled_dims_w = dims_vec.x * cell_size
        scaled_dims_h = dims_vec.y * cell_size
        color_player = (
            self.game_state.colors.get("PLAYER", pr.Color(0, 200, 255, 255))
            if is_self
            else self.game_state.colors.get("OTHER_PLAYER", pr.Color(255, 100, 0, 255))
        )

        # Draw label stacked above the entity: ID, Direction, Type ("Player")
        # compute center X of entity in pixels
        center_x = scaled_pos_x + scaled_dims_w / 2.0
        # compute top Y for labels (leave some padding)
        label_top_y = scaled_pos_y - 44  # three small lines above entity
        id_text = (
            entity_id if entity_id is not None else ("you" if is_self else "player")
        )
        dir_text = (
            direction.name if isinstance(direction, Direction) else str(direction)
        )
        type_text = "Player"

        # draw 3 stacked lines
        self._draw_entity_label(
            center_x,
            label_top_y,
            [str(id_text), str(dir_text), str(type_text)],
            font_size=12,
        )

        # draw the player rectangle (entity)
        pr.draw_rectangle_pro(
            pr.Rectangle(scaled_pos_x, scaled_pos_y, scaled_dims_w, scaled_dims_h),
            pr.Vector2(0, 0),
            0,
            color_player,
        )

    def _draw_bot_at(self, bot_entry, bot_id=None):
        """
        bot_entry is the dict with fields 'interp_pos','dims','behavior','direction'
        """
        cell_size = self.game_state.cell_size if self.game_state.cell_size > 0 else 12.0
        pos = bot_entry.get("interp_pos", bot_entry.get("pos_server", pr.Vector2(0, 0)))
        dims = bot_entry.get("dims", pr.Vector2(1, 1))
        behavior = bot_entry.get("behavior", "passive")
        direction = bot_entry.get("direction", Direction.NONE)

        scaled_pos_x = pos.x * cell_size
        scaled_pos_y = pos.y * cell_size
        scaled_w = dims.x * cell_size
        scaled_h = dims.y * cell_size

        # choose color based on behavior
        if behavior == "hostile":
            color_bot = self.game_state.colors.get(
                "ERROR_TEXT", pr.Color(255, 50, 50, 255)
            )
        else:
            color_bot = self.game_state.colors.get(
                "OTHER_PLAYER", pr.Color(100, 200, 100, 255)
            )

        # Draw label stacked above the bot: ID, Direction, Behavior
        center_x = scaled_pos_x + scaled_w / 2.0
        label_top_y = scaled_pos_y - 44
        id_text = bot_id if bot_id is not None else "bot"
        dir_text = (
            direction.name if isinstance(direction, Direction) else str(direction)
        )
        type_text = behavior

        self._draw_entity_label(
            center_x,
            label_top_y,
            [str(id_text), str(dir_text), str(type_text)],
            font_size=12,
        )

        pr.draw_rectangle_pro(
            pr.Rectangle(scaled_pos_x, scaled_pos_y, scaled_w, scaled_h),
            pr.Vector2(0, 0),
            0,
            color_bot,
        )

    # ---------- previous individual draw functions kept for compatibility but not directly used in z-sorted pass ----------
    def draw_bots(self):
        with self.game_state.mutex:
            if not self.game_state.bots:
                return
            for bot_id, bot in self.game_state.bots.items():
                self._draw_bot_at(bot, bot_id=bot_id)

    def draw_other_players(self):
        with self.game_state.mutex:
            for player_id, player_data in self.game_state.other_players.items():
                interp_pos = player_data.get(
                    "interp_pos", player_data.get("pos_server", pr.Vector2(0, 0))
                )
                dims = player_data.get("dims", pr.Vector2(1, 1))
                self._draw_player_at(
                    interp_pos,
                    dims,
                    False,
                    player_data.get("direction", Direction.NONE),
                    player_data.get("mode", ObjectLayerMode.IDLE),
                    entity_id=player_id,
                )

    def draw_player(self):
        self._draw_player_at(
            self.game_state.player_pos_interpolated,
            self.game_state.player_dims,
            True,
            self.game_state.player_direction,
            self.game_state.player_mode,
            entity_id=self.game_state.player_id or "you",
        )

    # ---------- draw entities sorted by world Y (y + height) ----------
    def draw_entities_sorted(self):
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
            try:
                if typ == "other":
                    # data contains interp_pos, dims, direction, mode
                    interp_pos = data.get(
                        "interp_pos", data.get("pos_server", pr.Vector2(0, 0))
                    )
                    dims = data.get("dims", pr.Vector2(1, 1))
                    self._draw_player_at(
                        interp_pos,
                        dims,
                        False,
                        data.get("direction", Direction.NONE),
                        data.get("mode", ObjectLayerMode.IDLE),
                        entity_id=_id,
                    )
                elif typ == "bot":
                    self._draw_bot_at(data, bot_id=_id)
                elif typ == "self":
                    self._draw_player_at(
                        data["pos"],
                        data["dims"],
                        True,
                        data.get("direction", Direction.NONE),
                        data.get("mode", ObjectLayerMode.IDLE),
                        entity_id=_id or "you",
                    )
            except Exception:
                # draw failures shouldn't crash rendering loop
                pass

    def draw_path(self):
        with self.game_state.mutex:
            if self.game_state.path:
                cell_size = (
                    self.game_state.cell_size if self.game_state.cell_size > 0 else 12.0
                )
                target_x, target_y = (
                    self.game_state.target_pos.x,
                    self.game_state.target_pos.y,
                )
                if target_x >= 0 and target_y >= 0:
                    pr.draw_rectangle_pro(
                        pr.Rectangle(
                            target_x * cell_size,
                            target_y * cell_size,
                            cell_size,
                            cell_size,
                        ),
                        pr.Vector2(0, 0),
                        0,
                        self.game_state.colors.get(
                            "TARGET", pr.Color(255, 255, 0, 255)
                        ),
                    )
                for p in self.game_state.path:
                    pr.draw_rectangle_pro(
                        pr.Rectangle(
                            p.x * cell_size, p.y * cell_size, cell_size, cell_size
                        ),
                        pr.Vector2(0, 0),
                        0,
                        self.game_state.colors.get("PATH", pr.Color(0, 255, 0, 128)),
                    )

    def draw_aoi_circle(self):
        with self.game_state.mutex:
            # Use player's center as AOI center so AOI is centered correctly regardless of player dims
            player_pos = self.game_state.player_pos_interpolated
            player_dims = self.game_state.player_dims
            cell_size = (
                self.game_state.cell_size if self.game_state.cell_size > 0 else 12.0
            )
            # compute center in pixels
            center_x = (player_pos.x + player_dims.x / 2.0) * cell_size
            center_y = (player_pos.y + player_dims.y / 2.0) * cell_size
            aoi_radius = (
                self.game_state.aoi_radius if self.game_state.aoi_radius > 0 else 15.0
            )
            pr.draw_circle_v(
                pr.Vector2(center_x, center_y),
                aoi_radius * cell_size,
                self.game_state.colors.get("AOI", pr.Color(255, 0, 255, 51)),
            )

    def draw_foregrounds(self):
        cell_size = self.game_state.cell_size if self.game_state.cell_size > 0 else 12.0
        with self.game_state.mutex:
            for obj_id, obj_data in self.game_state.foregrounds.items():
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
                        "FOREGROUND", pr.Color(60, 140, 60, 220)
                    ),
                )

    def draw_dev_ui(self):
        # compute how much vertical HUD currently occupies (approx)
        hud_occupied = (1.0 - self.hud.slide_progress) * self.hud.bar_height
        dev_ui_h = max(80, int(self.screen_height - hud_occupied))

        # top-left dev UI background (height adjusted)
        pr.draw_rectangle_pro(
            pr.Rectangle(0, 0, 450, dev_ui_h),
            pr.Vector2(0, 0),
            0,
            pr.fade(pr.BLACK, 0.4) if hasattr(pr, "fade") else pr.Color(0, 0, 0, 100),
        )
        # Replace "DEV UI" label with current FPS
        fps_text = f"{pr.get_fps()} FPS"
        pr.draw_text_ex(
            pr.get_font_default(),
            fps_text,
            pr.Vector2(10, 10),
            20,
            1,
            self.game_state.colors.get("DEBUG_TEXT", pr.Color(220, 220, 220, 255)),
        )
        with self.game_state.mutex:
            player_id = (
                self.game_state.player_id if self.game_state.player_id else "N/A"
            )
            player_map_id = self.game_state.player_map_id
            player_mode = self.game_state.player_mode.name
            player_dir = self.game_state.player_direction.name
            target_pos = self.game_state.target_pos
            download_kbps = self.dev_ui.download_kbps
            upload_kbps = self.dev_ui.upload_kbps
            error_msg = self.game_state.last_error_message
            player_pos_ui = self.game_state.player_pos_interpolated

            text_lines = [
                f"Player ID: {player_id}",
                f"Map ID: {player_map_id}",
                f"Mode: {player_mode} | Direction: {player_dir}",
                f"Pos: ({player_pos_ui.x:.2f}, {player_pos_ui.y:.2f})",
                f"Target: ({target_pos.x:.0f}, {target_pos.y:.0f})",
                f"Download: {download_kbps:.2f} kbps | Upload: {upload_kbps:.2f} kbps",
                f"SumStatsLimit: {self.game_state.sum_stats_limit}",
                f"ActiveStatsSum: {self.hud.active_stats_sum()}",
                f"ActiveItems: {len(self.hud.active_items())}",
            ]

            y_offset = 30
            for line in text_lines:
                pr.draw_text_ex(
                    pr.get_font_default(),
                    line,
                    pr.Vector2(10, y_offset),
                    18,
                    1,
                    self.game_state.colors.get("UI_TEXT", pr.Color(255, 255, 255, 255)),
                )
                y_offset += 20

            if error_msg:
                pr.draw_text_ex(
                    pr.get_font_default(),
                    f"Error: {error_msg}",
                    pr.Vector2(10, dev_ui_h - 30),
                    18,
                    1,
                    self.game_state.colors.get(
                        "ERROR_TEXT", pr.Color(255, 50, 50, 255)
                    ),
                )

    # ---------- HUD utilities (with slide/collapse) ----------
    def _hud_bar_rect(self):
        # returns (x, y, w, h) in screen coords using slide progress
        x = 0
        h = self.hud.bar_height
        # base Y if fully visible
        base_y = self.screen_height - h
        # when fully hidden (progress=1), top of HUD will be screen_height (i.e., out of view)
        hidden_y = self.screen_height
        # linear interpolate between base_y and hidden_y based on progress
        y = pr.lerp(base_y, hidden_y, self.hud.slide_progress)
        w = self.screen_width
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

    def draw_hud_bar(self, mouse_pos):
        # If HUD is fully hidden (progress near 1.0), do NOT draw the bar.
        # Still compute total widths so scroll clamping logic can use them.
        x, y, w, h = self._hud_bar_rect()

        inner_x = x + self.hud.bar_padding
        inner_y = y + (h - self.hud.item_h) / 2
        inner_w = w - (self.hud.bar_padding * 2)

        # total width of items
        count = len(self.hud.items)
        total_w = count * self.hud.item_w + (count - 1) * self.hud.item_spacing

        # clamp scroll range
        max_scroll = max(0, total_w - inner_w)
        if self.hud.scroll_x > 0:
            self.hud.scroll_x = 0
        if self.hud.scroll_x < -max_scroll:
            self.hud.scroll_x = -max_scroll

        # If fully hidden, skip drawing the bar entirely (only toggle remains visible)
        if self.hud.slide_progress >= 0.999:
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

        offset = inner_x + self.hud.scroll_x

        hovered_index = None
        for idx, item in enumerate(self.hud.items):
            bx = offset + idx * (self.hud.item_w + self.hud.item_spacing)
            by = inner_y
            # compute hover in screen coords
            hovered = (
                mouse_pos.x >= bx
                and mouse_pos.x <= bx + self.hud.item_w
                and mouse_pos.y >= by
                and mouse_pos.y <= by + self.hud.item_h
            )
            if hovered:
                hovered_index = idx
            # draw item
            self.draw_hud_item_button(
                bx, by, self.hud.item_w, self.hud.item_h, item, hovered
            )

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

    def draw_hud_view(self):
        """
        Draw the item view (English text). The view area adapts to the HUD slide state
        so it never overlaps with the HUD bar while the HUD is visible.
        """
        if self.hud.view_selected is None:
            self.hud.view_button_rect = None
            return
        item = self.hud.items[self.hud.view_selected]

        # compute view area (full width, height minus hud_bar_height occupied portion)
        hud_occupied = (1.0 - self.hud.slide_progress) * self.hud.bar_height
        view_x = 0
        view_y = 0
        view_w = self.screen_width
        view_h = int(self.screen_height - hud_occupied)

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
        current_active_sum = self.hud.active_stats_sum()
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
        elif len(self.hud.active_items()) >= 4 and not item.get("isActive"):
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
        close_x = self.screen_width - self.hud.close_w - 12
        close_y = 12
        self.draw_hud_small_button(
            close_x, close_y, self.hud.close_w, self.hud.close_h, "✕"
        )

        # Activation toggle (if activable) - reflect if activation would be allowed
        btn_w = 140
        btn_h = 40
        btn_x = self.screen_width - margin - btn_w
        btn_y = start_y + 10  # below title area
        if item.get("isActivable"):
            label = "Deactivate" if item.get("isActive") else "Activate"
            # check activation viability and show disabled visual if not allowed
            ok, reason = (
                self.hud.can_activate_item(item, self.game_state.sum_stats_limit)
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
            self.hud.view_button_rect = (btn_x, btn_y, btn_w, btn_h)
        else:
            self.hud.view_button_rect = None

    def draw_hud_alert(self):
        if not self.hud.alert_text or time.time() > self.hud.alert_until:
            return
        # draw centered top small alert using draw_rectangle_pro
        w = min(600, int(self.screen_width * 0.75))
        h = 44
        x = (self.screen_width - w) / 2
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
        tw = pr.measure_text(self.hud.alert_text, ts)
        pr.draw_text_ex(
            pr.get_font_default(),
            self.hud.alert_text,
            pr.Vector2(x + (w / 2) - (tw / 2), y + (h / 2) - (ts / 2)),
            ts,
            1,
            pr.Color(255, 255, 255, 255),
        )

    def draw_hud_toggle(self, mouse_pos):
        """
        Draw the toggle button on top of everything (so it overlaps the dev UI if needed).
        Use ▲ (open) / ▼ (close) symbols for clarity.
        The toggle y-position interpolates with hud_slide_progress so it has the same transition.
        """
        btn_w = 72
        btn_h = 22
        btn_x = (self.screen_width / 2) - (btn_w / 2)

        # compute toggle Y so it follows HUD transition:
        # when HUD visible (progress=0): place the toggle just above the HUD.
        # when HUD hidden (progress=1): place the toggle at bottom edge.
        hud_x, hud_y, hud_w, hud_h = self._hud_bar_rect()
        btn_y_when_visible = hud_y - btn_h - 8
        btn_y_when_hidden = self.screen_height - btn_h - 8
        # interpolate based on same progress so toggle moves with HUD
        btn_y = pr.lerp(btn_y_when_visible, btn_y_when_hidden, self.hud.slide_progress)

        # store rect for click detection
        self.hud.toggle_rect = (btn_x, btn_y, btn_w, btn_h)

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
        arrow = "▲" if self.hud.collapsed else "▼"
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

    # ---------- game loop ----------
    def run_game_loop(self):
        # use server fps if available, else fallback 60
        target_fps = self.game_state.fps if self.game_state.fps > 0 else 60

        last_download_check_time = time.time()
        while not pr.window_should_close() and self.ws_client.is_running:
            now = time.time()
            dt = (
                now - self._last_frame_time
                if self._last_frame_time
                else (1.0 / target_fps)
            )
            self._last_frame_time = now

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
            hx, hy, hw, hh = self._hud_bar_rect()
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
                hovered_index, total_w, inner_w = self.draw_hud_bar(mouse_pos)
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
                    hovered_index, _, _ = self.draw_hud_bar(mouse_pos)
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
            if self.hud.view_open and mouse_pressed:
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
                    self.send_player_action(target_x, target_y)
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
            self.interpolate_player_position()
            self.interpolate_entities_positions()

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

            # update client-side click effects
            self.click_effect.update_click_pointers()

            # draw
            self.draw_game()

        print("Closing WebSocket...")
        if self.ws_client.ws:
            self.ws_client.ws.close()
        self.ws_client.is_running = False
        pr.close_window()

    # ---------- utilities ----------
    def draw_game(self):
        bg = self.game_state.colors.get("BACKGROUND", pr.Color(30, 30, 30, 255))
        pr.begin_drawing()
        pr.clear_background(bg)

        # ensure camera offset centered as a good practice before BeginMode2D
        try:
            if self.game_state.camera:
                try:
                    self.game_state.camera.offset = pr.Vector2(
                        self.screen_width / 2, self.screen_height / 2
                    )
                except Exception:
                    pass
            pr.begin_mode_2d(self.game_state.camera)
        except Exception:
            # If begin_mode_2d fails, skip world transforms to avoid crash
            pass

        # world drawing
        self.draw_grid_lines()
        self.draw_grid_objects()
        self.draw_entities_sorted()
        self.draw_path()
        self.draw_aoi_circle()
        self.draw_foregrounds()
        self.click_effect.draw_click_pointers()  # client-only effect

        try:
            pr.end_mode_2d()
        except Exception:
            pass

        # UI layer (screen coordinates)
        mouse_pos = pr.get_mouse_position()

        # If view open: draw view area (above hud_bar). HUD bar will remain visible below.
        if self.hud.view_open and self.hud.view_selected is not None:
            self.draw_hud_view()

        # HUD bar (draw only if not fully hidden)
        hovered_index, total_w, inner_w = self.draw_hud_bar(mouse_pos)

        # Developer UI (if enabled by server) - now adjusted so it doesn't overlap HUD
        if self.game_state.dev_ui and self.hud.view_selected is None:
            self.draw_dev_ui()

        # Draw toggle *after* dev UI to ensure it is on top and clickable
        self.draw_hud_toggle(mouse_pos)

        # draw any hud alerts
        self.draw_hud_alert()

        pr.end_drawing()


if __name__ == "__main__":
    client = NetworkClient()
    client.start()
