import pyray as pr


class DevUI:
    def __init__(self, game_state, hud):
        self.download_kbps = 0.0
        self.upload_kbps = 0.0
        self.hud = hud
        self.game_state = game_state

    def draw_dev_ui(self, screen_width, screen_height):
        # compute how much vertical HUD currently occupies (approx)
        hud_occupied = (1.0 - self.hud.slide_progress) * self.hud.bar_height
        dev_ui_h = max(80, int(screen_height - hud_occupied))

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
