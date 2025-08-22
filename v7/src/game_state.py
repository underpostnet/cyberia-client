import threading
import pyray as pr
from src.object_layer import Direction, ObjectLayerMode
import time

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
        # obstacles/portals/foregrounds keep previous format
        self.obstacles = {}
        self.foregrounds = {}
        self.portals = {}

        # player positions (player uses interpolation already)
        self.player_pos_interpolated = pr.Vector2(0, 0)
        self.player_pos_server = pr.Vector2(0, 0)
        self.player_pos_prev = pr.Vector2(0, 0)
        self.player_dims = pr.Vector2(1.0, 1.0)

        # other players: keyed by id -> dict with keys:
        # { pos_server:Vector2, pos_prev:Vector2, interp_pos:Vector2, dims:Vector2, direction:Direction, mode:ObjectLayerMode, last_update:float }
        self.other_players = {}

        # bots: similar structure keyed by bot_id
        self.bots = {}

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

        # per-player sum stats limit (from server)
        self.sum_stats_limit = 9999

        self.init_received = False

        # created once graphics are initialized
        self.camera = None

        # toggle for developer UI (from server)
        self.dev_ui = False
