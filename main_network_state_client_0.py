import pyray as pr
import random
from dataclasses import dataclass, field
from typing import Tuple, Dict, List
import math  # Import math for sqrt

# --- Constants ---
SCREEN_WIDTH = 800
SCREEN_HEIGHT = 600
TILE_SIZE = 32
TELEPORT_HOLD_TIME = 2.0
MAP_WIDTH = 1600
MAP_HEIGHT = 1200
AOI_RADIUS = 300.0  # Define the Area of Interest radius

Position = Tuple[float, float]

# --- Game Objects ---


@dataclass
class GameObject:
    x: float
    y: float
    width: float
    height: float
    passable: bool = False
    color: pr.Color = field(default_factory=lambda: pr.GRAY)

    def get_rect(self) -> pr.Rectangle:
        return pr.Rectangle(self.x, self.y, self.width, self.height)

    def get_center(self) -> Position:
        return self.x + self.width / 2, self.y + self.height / 2


@dataclass
class Player(GameObject):
    color: pr.Color = field(default_factory=lambda: pr.BLUE)
    stationary_time: float = 0.0
    speed: float = 150.0
    prev_pos: Position = field(default_factory=lambda: (0.0, 0.0))

    def __post_init__(self):
        self.prev_pos = (self.x, self.y)

    def update(self, delta_time: float, obstacles: List[GameObject]):
        move_x, move_y = 0, 0
        if pr.is_key_down(pr.KEY_RIGHT):
            move_x += 1
        if pr.is_key_down(pr.KEY_LEFT):
            move_x -= 1
        if pr.is_key_down(pr.KEY_DOWN):
            move_y += 1
        if pr.is_key_down(pr.KEY_UP):
            move_y -= 1

        self.prev_pos = (self.x, self.y)
        new_x = self.x + move_x * self.speed * delta_time
        new_y = self.y + move_y * self.speed * delta_time
        new_rect = pr.Rectangle(new_x, new_y, self.width, self.height)

        can_move = True
        # Only check collisions with objects within a slightly larger AOI
        # to ensure smooth movement near the edge of the rendering AOI
        aoi_collision_radius = AOI_RADIUS * 1.2  # Slightly larger for collision
        player_center_x, player_center_y = self.get_center()

        for obj in obstacles:
            obj_center_x, obj_center_y = obj.get_center()
            distance_sq = (player_center_x - obj_center_x) ** 2 + (
                player_center_y - obj_center_y
            ) ** 2
            if (
                distance_sq < aoi_collision_radius**2
            ):  # Check if object is within collision AOI
                if not obj.passable and pr.check_collision_recs(
                    new_rect, obj.get_rect()
                ):
                    can_move = False
                    break

        if can_move:
            if abs(new_x - self.x) < 0.1 and abs(new_y - self.y) < 0.1:
                self.stationary_time += delta_time
            else:
                self.stationary_time = 0.0
            self.x, self.y = new_x, new_y
        else:
            self.stationary_time = 0.0


# --- Portal ---


@dataclass
class Portal(GameObject):
    dest_map: int
    dest_x: float
    dest_y: float

    passable: bool = field(init=False)
    color: pr.Color = field(init=False)

    def __post_init__(self):
        self.passable = True
        self.color = pr.GREEN


@dataclass
class Rock(GameObject):
    passable: bool = False
    color: pr.Color = field(default_factory=lambda: pr.BROWN)

    def __post_init__(self):
        self.width = TILE_SIZE
        self.height = TILE_SIZE


@dataclass
class Bush(GameObject):
    passable: bool = True
    color: pr.Color = field(default_factory=lambda: pr.DARKGREEN)

    def __post_init__(self):
        self.width = TILE_SIZE
        self.height = TILE_SIZE


# --- Camera ---


class Camera:
    def __init__(self, target: GameObject):
        self.target = target
        self.cam = pr.Camera2D()
        self.cam.offset = pr.Vector2(SCREEN_WIDTH // 2, SCREEN_HEIGHT // 2)
        self.cam.zoom = 1.0
        self.cam.target = pr.Vector2(
            self.target.x + self.target.width // 2,
            self.target.y + self.target.height // 2,
        )

    def update(self):
        target_pos = pr.Vector2(
            self.target.x + self.target.width // 2,
            self.target.y + self.target.height // 2,
        )
        self.cam.target = pr.Vector2(
            self.cam.target.x + (target_pos.x - self.cam.target.x) * 0.05,
            self.cam.target.y + (target_pos.y - self.cam.target.y) * 0.05,
        )

    def begin_mode(self):
        pr.begin_mode_2d(self.cam)

    def end_mode(self):
        pr.end_mode_2d()


# --- Map Generation ---


def generate_objects(num_objects: int, width: int, height: int) -> List[GameObject]:
    objs = []
    for _ in range(num_objects):
        x, y = random.randint(0, width - TILE_SIZE), random.randint(
            0, height - TILE_SIZE
        )
        if random.random() < 0.5:
            objs.append(Rock(x=float(x), y=float(y), width=TILE_SIZE, height=TILE_SIZE))
        else:
            objs.append(Bush(x=float(x), y=float(y), width=TILE_SIZE, height=TILE_SIZE))
    return objs


# --- Game State ---

# In a real large-scale game, 'maps' would not hold ALL objects for ALL maps in memory.
# Instead, it would be a mechanism to load/unload chunks or cells of objects as the player moves.
# For this example, we'll keep all map objects in memory for simplicity,
# but only process/render those within the AOI.
maps: Dict[int, Dict[str, List[GameObject]]] = {
    1: {
        "portals": [
            Portal(
                x=300.0,
                y=200.0,
                width=TILE_SIZE,
                height=TILE_SIZE,
                dest_map=2,
                dest_x=100.0,
                dest_y=100.0,
            ),
            Portal(
                x=700.0,
                y=500.0,
                width=TILE_SIZE,
                height=TILE_SIZE,
                dest_map=1,
                dest_x=50.0,
                dest_y=50.0,
            ),
        ],
        "obstacles": [],
    },
    2: {
        "portals": [
            Portal(
                x=100.0,
                y=100.0,
                width=TILE_SIZE,
                height=TILE_SIZE,
                dest_map=1,
                dest_x=300.0,
                dest_y=200.0,
            ),
            Portal(
                x=900.0,
                y=800.0,
                width=TILE_SIZE,
                height=TILE_SIZE,
                dest_map=2,
                dest_x=200.0,
                dest_y=200.0,
            ),
        ],
        "obstacles": [],
    },
}

maps[1]["obstacles"] = generate_objects(200, MAP_WIDTH, MAP_HEIGHT)
maps[2]["obstacles"] = generate_objects(200, MAP_WIDTH, MAP_HEIGHT)

current_map_id = 1
player = Player(x=50.0, y=50.0, width=TILE_SIZE, height=TILE_SIZE)
camera = Camera(player)

collided_portal_info: str = "None"
portal_hold_time_display: float = 0.0


# --- Main Game Loop ---


def main():
    global current_map_id, collided_portal_info, portal_hold_time_display
    pr.init_window(SCREEN_WIDTH, SCREEN_HEIGHT, b"RPG Map Portal System with AOI")
    pr.set_target_fps(60)

    while not pr.window_should_close():
        delta_time = pr.get_frame_time()
        camera.update()

        # Get player's center for AOI calculations
        player_center_x, player_center_y = player.get_center()

        # Filter obstacles and portals within AOI for collision and rendering
        # We use AOI_RADIUS for rendering, and a slightly larger radius for collision
        # and portal checks to prevent objects popping in/out too abruptly.
        # For simplicity, we'll use AOI_RADIUS for all filtering here.
        active_obstacles = []
        active_portals = []
        rendered_object_count = 0

        for obj in maps[current_map_id]["obstacles"]:
            obj_center_x, obj_center_y = obj.get_center()
            distance_sq = (player_center_x - obj_center_x) ** 2 + (
                player_center_y - obj_center_y
            ) ** 2
            if distance_sq < AOI_RADIUS**2:
                active_obstacles.append(obj)

        for portal in maps[current_map_id]["portals"]:
            portal_center_x, portal_center_y = portal.get_center()
            distance_sq = (player_center_x - portal_center_x) ** 2 + (
                player_center_y - portal_center_y
            ) ** 2
            if distance_sq < AOI_RADIUS**2:  # Use AOI_RADIUS for portal checks too
                active_portals.append(portal)

        # Update player with only active obstacles for collision detection
        player.update(delta_time, active_obstacles)

        collided_portal_info = "None"
        portal_hold_time_display = 0.0

        for portal in active_portals:  # Only check portals within AOI
            if pr.check_collision_recs(player.get_rect(), portal.get_rect()):
                collided_portal_info = f"Portal to Map {portal.dest_map} ({int(portal.dest_x)},{int(portal.dest_y)})"
                portal_hold_time_display = player.stationary_time
                if player.stationary_time >= TELEPORT_HOLD_TIME:
                    current_map_id = portal.dest_map
                    player.x, player.y = portal.dest_x, portal.dest_y
                    player.stationary_time = 0.0
                    break

        pr.begin_drawing()
        pr.clear_background(pr.RAYWHITE)
        camera.begin_mode()

        # Draw only objects within the AOI
        for obj in active_obstacles:
            pr.draw_rectangle_rec(obj.get_rect(), obj.color)
            rendered_object_count += 1
        for portal in active_portals:
            pr.draw_rectangle_rec(portal.get_rect(), portal.color)
            rendered_object_count += 1
        pr.draw_rectangle_rec(player.get_rect(), player.color)

        # Draw the AOI circle for visualization
        pr.draw_circle_lines(
            int(player_center_x), int(player_center_y), AOI_RADIUS, pr.BLUE
        )

        # The map boundary drawing should still consider the full map, not just AOI
        all_x = (
            [obj.x for obj in maps[current_map_id]["obstacles"]]
            + [p.x for p in maps[current_map_id]["portals"]]
            + [player.x]
        )
        all_y = (
            [obj.y for obj in maps[current_map_id]["obstacles"]]
            + [p.y for p in maps[current_map_id]["portals"]]
            + [player.y]
        )
        # Ensure there are elements before calling min/max
        if all_x and all_y:
            min_x, max_x = int(min(all_x)), int(max(all_x) + TILE_SIZE)
            min_y, max_y = int(min(all_y)), int(max(all_y) + TILE_SIZE)
            pr.draw_rectangle_lines(min_x, min_y, max_x - min_x, max_y - min_y, pr.RED)

        camera.end_mode()

        # --- UI ---
        pr.draw_text(f"Map: {current_map_id}", 10, 10, 20, pr.DARKGRAY)
        # Display the count of rendered objects, not all loaded objects
        pr.draw_text(
            f"Rendered objects: {rendered_object_count}", 10, 35, 20, pr.DARKGRAY
        )
        # Memory calculation now reflects only rendered objects for a more accurate representation
        kb_usage = (
            rendered_object_count * 16 / 1024
        )  # Assuming 16 bytes per object for rough estimate
        pr.draw_text(f"Memory (KB, rendered): {kb_usage:.1f}", 10, 60, 20, pr.DARKGRAY)
        if collided_portal_info != "None":
            pr.draw_text(
                f"Hold for {TELEPORT_HOLD_TIME:.1f}s: {portal_hold_time_display:.1f}s",
                10,
                85,
                20,
                pr.BLUE,
            )
        else:
            pr.draw_text("Hold for portal: Not colliding", 10, 85, 20, pr.DARKGRAY)

        # --- Minimap ---
        minimap_size = 200
        mm_x, mm_y = SCREEN_WIDTH - minimap_size - 10, SCREEN_HEIGHT - minimap_size - 10
        pr.draw_rectangle(mm_x, mm_y, minimap_size, minimap_size, pr.LIGHTGRAY)
        pr.draw_rectangle_lines(mm_x, mm_y, minimap_size, minimap_size, pr.DARKGRAY)
        scale_x, scale_y = minimap_size / MAP_WIDTH, minimap_size / MAP_HEIGHT
        # Minimap still shows all objects for context, not just AOI
        for obj in maps[current_map_id]["obstacles"]:
            pr.draw_rectangle(
                mm_x + int(obj.x * scale_x),
                mm_y + int(obj.y * scale_y),
                2,
                2,
                obj.color,
            )
        for portal in maps[current_map_id]["portals"]:
            pr.draw_rectangle(
                mm_x + int(portal.x * scale_x),
                mm_y + int(portal.y * scale_y),
                4,
                4,
                portal.color,
            )
        pr.draw_circle(
            mm_x + int(player.x * scale_x),
            mm_y + int(player.y * scale_y),
            3,
            player.color,
        )

        pr.end_drawing()

    pr.close_window()


if __name__ == "__main__":
    main()
