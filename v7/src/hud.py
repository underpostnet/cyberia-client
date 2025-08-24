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
