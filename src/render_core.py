import ctypes

import pyray as pr


class RenderCore:
    def __init__(self, client):
        self.client = client

    def draw_player_location_info(self):
        """If dev_ui is inactive, only the current map and its current coordinates should be displayed."""
        font_size = 18
        padding = 10
        main_color = pr.YELLOW
        shadow_color = pr.BLACK

        with self.client.game_state.mutex:
            map_id = self.client.game_state.player.map_id
            pos = self.client.game_state.player.interp_pos

        map_text = f"Map: {map_id}"
        pos_text = f"({pos.x:.1f}, {pos.y:.1f})"

        # Draw shadow text
        pr.draw_text(map_text, padding + 1, padding + 1, font_size, shadow_color)
        # Draw main text
        pr.draw_text(map_text, padding, padding, font_size, main_color)

        y_offset = padding + font_size + 5
        pr.draw_text(pos_text, padding + 1, y_offset + 1, font_size, shadow_color)
        pr.draw_text(pos_text, padding, y_offset, font_size, main_color)

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
            self.client.game_state.default_width_screen_factor
            if self.client.game_state.default_width_screen_factor is not None
            else 0.5
        )
        h_factor = (
            self.client.game_state.default_height_screen_factor
            if self.client.game_state.default_height_screen_factor is not None
            else 0.5
        )

        # Compute window size
        sw = max(640, int(monitor_w * float(w_factor)))
        sh = max(480, int(monitor_h * float(h_factor)))

        # ensure we don't exceed monitor
        sw = min(sw, monitor_w)
        sh = min(sh, monitor_h)

        self.client.screen_width = sw
        self.client.screen_height = sh

        print(
            f"Monitor detected: {monitor_w}x{monitor_h} -> Window: {self.client.screen_width}x{self.client.screen_height}"
        )

        # Initialize pyray window and fps
        pr.set_config_flags(pr.ConfigFlags.FLAG_VSYNC_HINT)
        pr.init_window(
            self.client.screen_width, self.client.screen_height, "MMO Client"
        )

        target_fps = (
            self.client.game_state.fps if self.client.game_state.fps > 0 else 60
        )
        pr.set_target_fps(target_fps)

        # Camera creation:
        try:
            cam_zoom = (
                float(self.client.game_state.camera_zoom)
                if self.client.game_state.camera_zoom is not None
                else 1.0
            )
        except Exception:
            cam_zoom = 1.0

        # initial target = player center position * cell_size (may be zero)
        try:
            player_center_x = (
                self.client.game_state.player.interp_pos.x
                + self.client.game_state.player.dims.x / 2.0
            ) * (
                self.client.game_state.cell_size
                if self.client.game_state.cell_size > 0
                else 12.0
            )
            player_center_y = (
                self.client.game_state.player.interp_pos.y
                + self.client.game_state.player.dims.y / 2.0
            ) * (
                self.client.game_state.cell_size
                if self.client.game_state.cell_size > 0
                else 12.0
            )
            initial_target = pr.Vector2(player_center_x, player_center_y)
        except Exception:
            initial_target = pr.Vector2(0, 0)

        offset = pr.Vector2(self.client.screen_width / 2, self.client.screen_height / 2)
        # create camera with offset first (many python bindings expect (offset, target, rot, zoom))
        try:
            self.client.game_state.camera = pr.Camera2D(
                offset, initial_target, 0.0, cam_zoom
            )
        except Exception:
            # fallback in case signature differs; try swapping args
            try:
                self.client.game_state.camera = pr.Camera2D(
                    initial_target, offset, 0.0, cam_zoom
                )
            except Exception:
                # final fallback: construct with zeros then assign attributes if possible
                self.client.game_state.camera = pr.Camera2D(
                    pr.Vector2(0, 0), pr.Vector2(0, 0), 0.0, cam_zoom
                )
                try:
                    self.client.game_state.camera.offset = offset
                    self.client.game_state.camera.target = initial_target
                except Exception:
                    pass

        # store camera offset for our use (we will keep camera.offset centered)
        try:
            # ensure offset is set correctly
            self.client.game_state.camera.offset = pr.Vector2(
                self.client.screen_width / 2, self.client.screen_height / 2
            )
        except Exception:
            pass

    def draw_path(self):
        with self.client.game_state.mutex:
            if self.client.game_state.player.path:
                cell_size = (
                    self.client.game_state.cell_size
                    if self.client.game_state.cell_size > 0
                    else 12.0
                )
                target_x, target_y = (
                    self.client.game_state.player.target_pos.x,
                    self.client.game_state.player.target_pos.y,
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
                        self.client.game_state.colors.get(
                            "TARGET", pr.Color(255, 255, 0, 255)
                        ),
                    )
                for p in self.client.game_state.player.path:
                    pr.draw_rectangle_pro(
                        pr.Rectangle(
                            p.x * cell_size, p.y * cell_size, cell_size, cell_size
                        ),
                        pr.Vector2(0, 0),
                        0,
                        self.client.game_state.colors.get(
                            "PATH", pr.Color(0, 255, 0, 128)
                        ),
                    )

    def draw_aoi_circle(self):
        with self.client.game_state.mutex:
            # Use player's center as AOI center so AOI is centered correctly regardless of player dims
            player_pos = self.client.game_state.player.interp_pos
            player_dims = self.client.game_state.player.dims
            cell_size = (
                self.client.game_state.cell_size
                if self.client.game_state.cell_size > 0
                else 12.0
            )
            # compute center in pixels
            center_x = (player_pos.x + player_dims.x / 2.0) * cell_size
            center_y = (player_pos.y + player_dims.y / 2.0) * cell_size
            aoi_radius = (
                self.client.game_state.aoi_radius
                if self.client.game_state.aoi_radius > 0
                else 15.0
            )
            pr.draw_circle_v(
                pr.Vector2(center_x, center_y),
                aoi_radius * cell_size,
                self.client.game_state.colors.get("AOI", pr.Color(255, 0, 255, 51)),
            )

    def draw_foregrounds(self):
        cell_size = (
            self.client.game_state.cell_size
            if self.client.game_state.cell_size > 0
            else 12.0
        )
        with self.client.game_state.mutex:
            for obj_id, obj_data in self.client.game_state.foregrounds.items():
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
                    self.client.game_state.colors.get(
                        "FOREGROUND", pr.Color(60, 140, 60, 220)
                    ),
                )

    def draw_game(self):
        print("[DEBUG RENDER] draw_game() started")
        bg = self.client.game_state.colors.get("BACKGROUND", pr.Color(30, 30, 30, 255))
        print("[DEBUG RENDER] Got background color")
        pr.begin_drawing()
        print("[DEBUG RENDER] begin_drawing() called")
        pr.clear_background(bg)
        print("[DEBUG RENDER] clear_background() called")

        # ensure camera offset centered as a good practice before BeginMode2D
        print("[DEBUG RENDER] Setting up camera...")
        try:
            if self.client.game_state.camera:
                try:
                    self.client.game_state.camera.offset = pr.Vector2(
                        self.client.screen_width / 2, self.client.screen_height / 2
                    )
                    print("[DEBUG RENDER] Camera offset set")
                except Exception:
                    print("[DEBUG RENDER] Failed to set camera offset")
                    pass
            print("[DEBUG RENDER] Calling begin_mode_2d()...")
            pr.begin_mode_2d(self.client.game_state.camera)
            print("[DEBUG RENDER] begin_mode_2d() completed")
        except Exception as e:
            # If begin_mode_2d fails, skip world transforms to avoid crash
            print(f"[DEBUG RENDER] begin_mode_2d() failed: {e}")
            pass

        # world drawing
        print("[DEBUG RENDER] Drawing grid background...")
        self.client.grid_render.draw_grid_background()
        print("[DEBUG RENDER] Drawing grid floors...")
        self.client.grid_render.draw_grid_floors()
        print("[DEBUG RENDER] Drawing grid lines...")
        self.client.grid_render.draw_grid_lines()
        print("[DEBUG RENDER] Drawing grid objects...")
        self.client.grid_render.draw_grid_objects()
        print("[DEBUG RENDER] Drawing entities...")
        self.client.entity_render.draw_entities_sorted(
            self.client.entity_player_render, self.client.entity_bot_render
        )
        print("[DEBUG RENDER] Drawing path...")
        self.draw_path()
        if self.client.game_state.dev_ui:
            print("[DEBUG RENDER] Drawing AOI circle...")
            self.draw_aoi_circle()
        print("[DEBUG RENDER] Drawing foregrounds...")
        self.draw_foregrounds()
        print("[DEBUG RENDER] Drawing click pointers...")
        self.client.click_effect.draw_click_pointers()  # client-only effect
        print("[DEBUG RENDER] Drawing floating text...")
        self.client.floating_text_manager.draw()
        print("[DEBUG RENDER] World drawing completed")

        try:
            print("[DEBUG RENDER] Calling end_mode_2d()...")
            pr.end_mode_2d()
            print("[DEBUG RENDER] end_mode_2d() completed")
        except Exception as e:
            print(f"[DEBUG RENDER] end_mode_2d() failed: {e}")
            pass

        # UI layer (screen coordinates)
        print("[DEBUG RENDER] Getting mouse position for UI...")
        mouse_pos = pr.get_mouse_position()
        print(f"[DEBUG RENDER] Mouse position: ({mouse_pos.x}, {mouse_pos.y})")

        # If view open: draw view area (above hud_bar). HUD bar will remain visible below.
        print("[DEBUG RENDER] Checking if HUD view is open...")
        if self.client.hud.view_open and self.client.hud.view_selected is not None:
            print("[DEBUG RENDER] Drawing HUD view...")
            self.client.hud.draw_hud_view(
                self.client.screen_width, self.client.screen_height
            )
            print("[DEBUG RENDER] HUD view drawn")

        # Sub-HUD bar (drawn on top of view, below main HUD)
        print("[DEBUG RENDER] Drawing Sub-HUD...")
        self.client.hud.sub_hud.draw(
            mouse_pos, self.client.screen_width, self.client.screen_height
        )
        print("[DEBUG RENDER] Sub-HUD drawn")

        # HUD bar (draw only if not fully hidden)
        print("[DEBUG RENDER] Drawing HUD bar...")
        hovered_index, total_w, inner_w = self.client.hud.draw_hud_bar(
            mouse_pos, self.client.screen_width, self.client.screen_height
        )
        print(f"[DEBUG RENDER] HUD bar drawn (hovered_index={hovered_index})")

        # Developer UI (if enabled by server) - now adjusted so it doesn't overlap HUD
        print("[DEBUG RENDER] Checking dev UI...")
        if self.client.game_state.dev_ui and self.client.hud.view_selected is None:
            print("[DEBUG RENDER] Drawing dev UI...")
            self.client.dev_ui.draw_dev_ui(
                self.client.screen_width, self.client.screen_height
            )
            print("[DEBUG RENDER] Dev UI drawn")
        elif (
            not self.client.game_state.dev_ui and self.client.hud.view_selected is None
        ):
            print("[DEBUG RENDER] Drawing player location info...")
            self.draw_player_location_info()
            print("[DEBUG RENDER] Player location info drawn")

        # Draw toggle *after* dev UI to ensure it is on top and clickable
        print("[DEBUG RENDER] Drawing HUD toggle...")
        self.client.hud.draw_hud_toggle(
            mouse_pos, self.client.screen_width, self.client.screen_height
        )
        print("[DEBUG RENDER] HUD toggle drawn")

        # draw any hud alerts
        print("[DEBUG RENDER] Drawing HUD alerts...")
        self.client.hud.draw_hud_alert(
            self.client.screen_width, self.client.screen_height
        )
        print("[DEBUG RENDER] HUD alerts drawn")

        print("[DEBUG RENDER] Calling end_drawing()...")
        pr.end_drawing()
        print("[DEBUG RENDER] draw_game() COMPLETED\n")
