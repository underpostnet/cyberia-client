import pyray as pr
import websocket
import threading
import json
import time
import math
import sys
import ctypes
from enum import Enum

WS_URL = "ws://localhost:8080/ws"


class Direction(Enum):
    UP = 0
    UP_RIGHT = 1
    RIGHT = 2
    DOWN_RIGHT = 3
    DOWN = 4
    DOWN_LEFT = 5
    LEFT = 6
    UP_LEFT = 7
    NONE = 8


class ObjectLayerMode(Enum):
    IDLE = 0
    WALKING = 1
    TELEPORTING = 2


class GameState:
    def __init__(self):
        self.mutex = threading.Lock()
        self.player_id = None
        self.player_map_id = 0
        self.player_mode = ObjectLayerMode.IDLE
        self.player_direction = Direction.NONE

        # Will be set by init_data
        self.grid_w = 0
        self.grid_h = 0
        self.cell_size = 0.0
        self.fps = 0
        self.interpolation_ms = 0
        self.aoi_radius = 0.0

        self.default_obj_width = 0.0
        self.default_obj_height = 0.0

        self.colors = {}

        # runtime
        self.obstacles = {}
        self.foregrounds = {}
        self.portals = {}
        self.player_pos_interpolated = pr.Vector2(0, 0)
        self.player_pos_server = pr.Vector2(0, 0)
        self.player_pos_prev = pr.Vector2(0, 0)
        self.player_dims = pr.Vector2(1.0, 1.0)
        self.other_players = {}
        self.path = []
        self.target_pos = pr.Vector2(-1, -1)
        self.last_update_time = time.time()
        self.last_error_message = ""
        self.error_display_time = 0.0
        self.download_size_bytes = 0
        self.upload_size_bytes = 0

        # graphics hints (from server)
        self.camera_smoothing = None
        self.camera_zoom = None
        self.default_width_screen_factor = None
        self.default_height_screen_factor = None

        self.init_received = False

        # created once graphics are initialized
        self.camera = None


class NetworkClient:
    def __init__(self):
        self.game_state = GameState()
        self.ws = None
        self.ws_thread = None
        self.is_running = True
        self.mutex = self.game_state.mutex
        self.download_kbps = 0.0
        self.upload_kbps = 0.0

        # runtime window (computed after init_data)
        self.screen_width = 1280
        self.screen_height = 800

        # client-only click effects (gfx_click_pointer)
        # Each item: {"pos": Vector2 (world coords, pixels), "t": created_time, "dur": seconds}
        self.click_effects = []

        # event to signal main thread to initialize graphics
        self.init_event = threading.Event()

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
        with self.mutex:
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

                    # mark and notify main thread
                    self.game_state.init_received = True
                    self.init_event.set()

                elif message_type == "aoi_update":
                    payload = data.get("payload")
                    if not payload:
                        return

                    player_data = payload.get("player")
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

                    # other players
                    visible_players_data = payload.get("visiblePlayers")
                    self.game_state.other_players = {}
                    if visible_players_data:
                        for player_id, p_data in visible_players_data.items():
                            if player_id != self.game_state.player_id:
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

                                self.game_state.other_players[player_id] = {
                                    "pos": pr.Vector2(pos.get("X"), pos.get("Y")),
                                    "dims": pr.Vector2(
                                        dims.get("Width"), dims.get("Height")
                                    ),
                                    "direction": dir_enum,
                                    "mode": mode_enum,
                                }

                    # visible objects
                    visible_objects_data = payload.get("visibleGridObjects")
                    self.game_state.obstacles = {}
                    self.game_state.portals = {}
                    self.game_state.foregrounds = {}  # reset foregrounds each update
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

            except json.JSONDecodeError as e:
                with self.mutex:
                    self.game_state.last_error_message = f"JSON Decode Error: {e}"
                    self.game_state.error_display_time = time.time()
                print(f"JSON Decode Error: {e}")
            except Exception as e:
                with self.mutex:
                    self.game_state.last_error_message = f"Error: {e}"
                    self.game_state.error_display_time = time.time()
                print(f"Error: {e}")

    def on_error(self, ws, error):
        with self.mutex:
            self.game_state.last_error_message = f"WebSocket Error: {error}"
            self.game_state.error_display_time = time.time()
        print(f"### WebSocket Error ###: {error}")

    def on_close(self, ws, close_status_code, close_msg):
        with self.mutex:
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

    # ---------- start / initialization ----------
    def start(self):
        print("Starting WebSocket client thread...")
        self.ws_thread = threading.Thread(target=self.run_websocket_thread, daemon=True)
        self.ws_thread.start()

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
        # Raylib Camera2D has fields: offset, target, rotation, zoom
        # offset -> screen center; target -> world coordinate to center on screen
        try:
            cam_zoom = (
                float(self.game_state.camera_zoom)
                if self.game_state.camera_zoom is not None
                else 1.0
            )
        except Exception:
            cam_zoom = 1.0

        # initial target = player position * cell_size (may be zero)
        try:
            initial_target = pr.Vector2(
                self.game_state.player_pos_interpolated.x
                * (
                    self.game_state.cell_size if self.game_state.cell_size > 0 else 12.0
                ),
                self.game_state.player_pos_interpolated.y
                * (
                    self.game_state.cell_size if self.game_state.cell_size > 0 else 12.0
                ),
            )
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
        if self.ws and self.ws.sock and self.ws.sock.connected:
            try:
                action_message = {
                    "type": "player_action",
                    "payload": {"targetX": target_x, "targetY": target_y},
                }
                self.ws.send(json.dumps(action_message))
                self.game_state.upload_size_bytes += len(json.dumps(action_message))
            except websocket.WebSocketConnectionClosedException:
                print("Cannot send message, connection is closed.")
            except Exception as e:
                print(f"Error sending message: {e}")

    # ---------- interpolation & drawing ----------
    def interpolate_player_position(self):
        with self.mutex:
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

    def draw_grid_lines(self):
        grid_w = self.game_state.grid_w if self.game_state.grid_w > 0 else 100
        grid_h = self.game_state.grid_h if self.game_state.grid_h > 0 else 100
        cell_size = self.game_state.cell_size if self.game_state.cell_size > 0 else 12.0
        map_w, map_h = grid_w * cell_size, grid_h * cell_size
        color_boundary = self.game_state.colors.get(
            "MAP_BOUNDARY", pr.Color(255, 255, 255, 255)
        )
        pr.draw_rectangle_lines_ex(pr.Rectangle(0, 0, map_w, map_h), 1, color_boundary)
        for i in range(grid_w):
            start_pos = pr.Vector2(i * cell_size, 0)
            end_pos = pr.Vector2(i * cell_size, map_h)
            pr.draw_line_ex(start_pos, end_pos, 1, pr.fade(color_boundary, 0.2))
        for j in range(grid_h):
            start_pos = pr.Vector2(0, j * cell_size)
            end_pos = pr.Vector2(map_w, j * cell_size)
            pr.draw_line_ex(start_pos, end_pos, 1, pr.fade(color_boundary, 0.2))

    def draw_player(self):
        player_dims = self.game_state.player_dims
        player_pos = self.game_state.player_pos_interpolated
        cell_size = self.game_state.cell_size if self.game_state.cell_size > 0 else 12.0
        scaled_pos_x = player_pos.x * cell_size
        scaled_pos_y = player_pos.y * cell_size
        scaled_dims_w = player_dims.x * cell_size
        scaled_dims_h = player_dims.y * cell_size
        color_player = self.game_state.colors.get("PLAYER", pr.Color(0, 200, 255, 255))
        pr.draw_rectangle_pro(
            pr.Rectangle(scaled_pos_x, scaled_pos_y, scaled_dims_w, scaled_dims_h),
            pr.Vector2(0, 0),
            0,
            color_player,
        )

    def draw_other_players(self):
        with self.mutex:
            cell_size = (
                self.game_state.cell_size if self.game_state.cell_size > 0 else 12.0
            )
            for player_id, player_data in self.game_state.other_players.items():
                pos = player_data["pos"]
                dims = player_data["dims"]
                direction = player_data.get("direction", Direction.NONE)
                scaled_pos_x = pos.x * cell_size
                scaled_pos_y = pos.y * cell_size
                scaled_dims_w = dims.x * cell_size
                scaled_dims_h = dims.y * cell_size
                color_other = self.game_state.colors.get(
                    "OTHER_PLAYER", pr.Color(255, 100, 0, 255)
                )
                pr.draw_rectangle_pro(
                    pr.Rectangle(
                        scaled_pos_x, scaled_pos_y, scaled_dims_w, scaled_dims_h
                    ),
                    pr.Vector2(0, 0),
                    0,
                    color_other,
                )
                dir_text = (
                    direction.name
                    if isinstance(direction, Direction)
                    else str(direction)
                )
                pr.draw_text_ex(
                    pr.get_font_default(),
                    dir_text,
                    pr.Vector2(scaled_pos_x, scaled_pos_y - 12),
                    10,
                    1,
                    self.game_state.colors.get("UI_TEXT", pr.Color(255, 255, 255, 255)),
                )

    def draw_grid_objects(self):
        cell_size = self.game_state.cell_size if self.game_state.cell_size > 0 else 12.0
        with self.mutex:
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
                label_pos = pr.Vector2(
                    (pos.x + dims.x / 2) * cell_size, (pos.y + dims.y / 2) * cell_size
                )
                pr.draw_text_pro(
                    pr.get_font_default(),
                    label,
                    label_pos,
                    pr.Vector2(pr.measure_text(label, 10) / 2, 5),
                    0,
                    10,
                    2,
                    self.game_state.colors.get(
                        "PORTAL_LABEL", pr.Color(240, 240, 240, 255)
                    ),
                )

    def draw_path(self):
        with self.mutex:
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
        with self.mutex:
            player_pos = self.game_state.player_pos_interpolated
            cell_size = (
                self.game_state.cell_size if self.game_state.cell_size > 0 else 12.0
            )
            aoi_radius = (
                self.game_state.aoi_radius if self.game_state.aoi_radius > 0 else 15.0
            )
            pr.draw_circle_v(
                pr.Vector2(player_pos.x * cell_size, player_pos.y * cell_size),
                aoi_radius * cell_size,
                self.game_state.colors.get("AOI", pr.Color(255, 0, 255, 51)),
            )

    # ----- client-only click pointers -----
    def add_click_pointer(self, world_pos):
        # world_pos is in world pixels (already converted by get_screen_to_world_2d)
        self.click_effects.append(
            {
                "pos": pr.Vector2(world_pos.x, world_pos.y),
                "t": time.time(),
                "dur": 0.75,
            }
        )

    def update_click_pointers(self):
        now = time.time()
        self.click_effects = [
            e for e in self.click_effects if (now - e["t"]) < e["dur"]
        ]

    def draw_click_pointers(self):
        now = time.time()
        base_color = self.game_state.colors.get("CLICK", pr.Color(255, 255, 255, 220))
        for e in self.click_effects:
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

    def draw_foregrounds(self):
        cell_size = self.game_state.cell_size if self.game_state.cell_size > 0 else 12.0
        with self.mutex:
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
        # top bar background
        pr.draw_rectangle(0, 0, 450, 160, pr.fade(pr.BLACK, 0.4))
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
        with self.mutex:
            player_id = (
                self.game_state.player_id if self.game_state.player_id else "N/A"
            )
            player_map_id = self.game_state.player_map_id
            player_mode = self.game_state.player_mode.name
            player_dir = self.game_state.player_direction.name
            target_pos = self.game_state.target_pos
            download_kbps = self.download_kbps
            upload_kbps = self.upload_kbps
            error_msg = self.game_state.last_error_message
            player_pos_ui = self.game_state.player_pos_interpolated

            text_lines = [
                f"Player ID: {player_id}",
                f"Map ID: {player_map_id}",
                f"Mode: {player_mode} | Direction: {player_dir}",
                f"Pos: ({player_pos_ui.x:.2f}, {player_pos_ui.y:.2f})",
                f"Target: ({target_pos.x:.0f}, {target_pos.y:.0f})",
                f"Download: {download_kbps:.2f} kbps | Upload: {upload_kbps:.2f} kbps",
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
                    pr.Vector2(10, self.screen_height - 30),
                    18,
                    1,
                    self.game_state.colors.get(
                        "ERROR_TEXT", pr.Color(255, 50, 50, 255)
                    ),
                )

    def run_game_loop(self):
        # use server fps if available, else fallback 60
        target_fps = self.game_state.fps if self.game_state.fps > 0 else 60

        last_download_check_time = time.time()
        while not pr.window_should_close() and self.is_running:
            # input
            if pr.is_mouse_button_pressed(pr.MOUSE_LEFT_BUTTON):
                mouse_pos = pr.get_mouse_position()
                world_pos = pr.get_screen_to_world_2d(mouse_pos, self.game_state.camera)
                target_x = world_pos.x / (
                    self.game_state.cell_size if self.game_state.cell_size > 0 else 12.0
                )
                target_y = world_pos.y / (
                    self.game_state.cell_size if self.game_state.cell_size > 0 else 12.0
                )
                target_x = math.floor(target_x)
                target_y = math.floor(target_y)
                self.send_player_action(target_x, target_y)
                # client-side click pointer effect
                self.add_click_pointer(world_pos)

            # bandwidth
            current_time = time.time()
            if current_time - last_download_check_time >= 1:
                with self.mutex:
                    self.download_kbps = (
                        self.game_state.download_size_bytes / 1024
                    ) * 8
                    self.upload_kbps = (self.game_state.upload_size_bytes / 1024) * 8
                    self.game_state.download_size_bytes = 0
                    self.game_state.upload_size_bytes = 0
                last_download_check_time = current_time

            # interpolation
            self.interpolate_player_position()

            # camera smoothing: compute desired world target and smooth camera.target to it
            try:
                cell_size = (
                    self.game_state.cell_size if self.game_state.cell_size > 0 else 12.0
                )
                desired = pr.Vector2(
                    self.game_state.player_pos_interpolated.x * cell_size,
                    self.game_state.player_pos_interpolated.y * cell_size,
                )
                if self.game_state.camera is None:
                    # safety: create default camera if missing
                    offset = pr.Vector2(self.screen_width / 2, self.screen_height / 2)
                    self.game_state.camera = pr.Camera2D(
                        offset,
                        desired,
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
                new_tx = pr.lerp(current_target.x, desired.x, smooth)
                new_ty = pr.lerp(current_target.y, desired.y, smooth)
                # apply
                try:
                    self.game_state.camera.target = pr.Vector2(new_tx, new_ty)
                    # ensure offset remains centered
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
                    except Exception:
                        pass
            except Exception:
                pass

            # update client-side click effects
            self.update_click_pointers()

            # draw
            self.draw_game()

        print("Closing WebSocket...")
        if self.ws:
            self.ws.close()
        self.is_running = False
        pr.close_window()

    # ---------- utilities ----------
    def draw_game(self):
        pr.begin_drawing()
        bg = self.game_state.colors.get("BACKGROUND", pr.Color(30, 30, 30, 255))
        pr.clear_background(bg)

        # begin mode 2d with our camera (Camera2D.offset must be screen center)
        try:
            pr.begin_mode_2d(self.game_state.camera)
        except Exception:
            # If begin_mode_2d fails, skip world transforms to avoid crash
            pass

        # world drawing
        self.draw_grid_lines()
        self.draw_grid_objects()
        self.draw_other_players()
        self.draw_player()
        self.draw_path()
        self.draw_aoi_circle()
        self.draw_click_pointers()  # client-only effect

        # Draw foregrounds last inside the world so they appear above players but below UI
        self.draw_foregrounds()

        try:
            pr.end_mode_2d()
        except Exception:
            pass

        # UI
        self.draw_dev_ui()
        pr.end_drawing()


if __name__ == "__main__":
    client = NetworkClient()
    client.start()
