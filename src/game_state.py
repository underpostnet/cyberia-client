import threading
import pyray as pr
from src.object_layer.object_layer import Direction, ObjectLayerMode
from typing import Dict, List
from src.entity_state import PlayerState, BotState, EntityState
import time


class GameState:
    def __init__(self):
        self.mutex = threading.Lock()
        self.player_id = None

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
        self.floors = {}

        # Main player state using the new dataclass
        self.player: PlayerState = PlayerState(
            id="",
            pos_server=pr.Vector2(0, 0),
            pos_prev=pr.Vector2(0, 0),
            interp_pos=pr.Vector2(0, 0),
            dims=pr.Vector2(1.0, 1.0),
            direction=Direction.NONE,
            mode=ObjectLayerMode.IDLE,
        )

        # other players: keyed by id -> EntityState
        self.other_players = {}

        # bots: keyed by bot_id -> BotState
        self.bots: dict[str, BotState] = {}
        self.last_update_time = time.time()

        # sub-hud state
        self.associated_item_ids: Dict[str, List[str]] = {}

        # sub-hud state
        self.associated_item_ids: Dict[str, List[str]] = {}
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
