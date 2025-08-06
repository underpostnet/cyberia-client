import pyray as pr
import random
from dataclasses import dataclass, field
from typing import Tuple, Dict, List
import math

# --- Constants ---
SCREEN_WIDTH = 800
SCREEN_HEIGHT = 600
TILE_SIZE = 32
TELEPORT_HOLD_TIME = 2.0
MAP_WIDTH = 1600
MAP_HEIGHT = 1200
AOI_RADIUS = 300.0

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
        """Returns the PyRay Rectangle for the game object."""
        return pr.Rectangle(self.x, self.y, self.width, self.height)

    def get_center(self) -> Position:
        """Returns the center coordinates of the game object."""
        return self.x + self.width / 2, self.y + self.height / 2


@dataclass
class Player(GameObject):
    color: pr.Color = field(default_factory=lambda: pr.BLUE)
    stationary_time: float = 0.0
    speed: float = 150.0
    prev_pos: Position = field(default_factory=lambda: (0.0, 0.0))

    def __post_init__(self):
        """Initializes the previous position after object creation."""
        self.prev_pos = (self.x, self.y)

    def update(self, delta_time: float, obstacles: List[GameObject]):
        """Updates the player's position based on input and handles collisions."""
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
        # Use a slightly larger AOI for collision checks to prevent "popping" issues
        aoi_collision_radius = AOI_RADIUS * 1.2
        player_center_x, player_center_y = self.get_center()

        for obj in obstacles:
            obj_center_x, obj_center_y = obj.get_center()
            distance_sq = (player_center_x - obj_center_x) ** 2 + (
                player_center_y - obj_center_y
            ) ** 2
            if distance_sq < aoi_collision_radius**2:
                # Player only collides with non-passable objects
                if not obj.passable and pr.check_collision_recs(
                    new_rect, obj.get_rect()
                ):
                    can_move = False
                    break

        if can_move:
            # Update stationary time if player is not moving significantly
            if abs(new_x - self.x) < 0.1 and abs(new_y - self.y) < 0.1:
                self.stationary_time += delta_time
            else:
                self.stationary_time = 0.0
            self.x, self.y = new_x, new_y
        else:
            self.stationary_time = 0.0


@dataclass
class Portal(GameObject):
    dest_map: int
    dest_portal_index: int  # Index of the destination portal in the target map
    spawn_radius: float = 100.0  # Radius around this portal's center for player spawn

    passable: bool = field(init=False)
    color: pr.Color = field(init=False)

    def __post_init__(self):
        """Initializes portal properties."""
        self.passable = True
        self.color = pr.GREEN


@dataclass
class Rock(GameObject):
    passable: bool = False
    color: pr.Color = field(default_factory=lambda: pr.BROWN)

    def __post_init__(self):
        """Initializes rock dimensions."""
        self.width = TILE_SIZE
        self.height = TILE_SIZE


@dataclass
class Bush(GameObject):
    passable: bool = True
    color: pr.Color = field(default_factory=lambda: pr.DARKGREEN)

    def __post_init__(self):
        """Initializes bush dimensions."""
        self.width = TILE_SIZE
        self.height = TILE_SIZE


# --- Camera ---
class GameCamera:
    def __init__(self, target: GameObject, map_width: int, map_height: int):
        self.target = target
        self.map_width = map_width
        self.map_height = map_height
        self.cam = pr.Camera2D()
        self.cam.offset = pr.Vector2(SCREEN_WIDTH // 2, SCREEN_HEIGHT // 2)
        self.cam.zoom = 1.0
        # Initialize camera target to player's initial position
        self.cam.target = pr.Vector2(
            self.target.x + self.target.width / 2,
            self.target.y + self.target.height / 2,
        )
        self.CAMERA_SMOOTHNESS = 0.05  # Lower value for smoother camera
        self.camera_shake_intensity = 0.0
        self.camera_shake_duration = 0.0

    def update(self, delta_time: float):
        """Updates the camera's position, applies clamping and shake."""
        # Desired camera target based on player's center
        desired_target_x = self.target.x + self.target.width / 2
        desired_target_y = self.target.y + self.target.height / 2

        # Smooth camera movement using linear interpolation (LERP)
        self.cam.target.x = pr.lerp(
            self.cam.target.x, desired_target_x, self.CAMERA_SMOOTHNESS
        )
        self.cam.target.y = pr.lerp(
            self.cam.target.y, desired_target_y, self.CAMERA_SMOOTHNESS
        )

        # Clamp camera to map boundaries
        # Calculate the camera's effective viewable area in world coordinates
        half_screen_width = SCREEN_WIDTH / 2 / self.cam.zoom
        half_screen_height = SCREEN_HEIGHT / 2 / self.cam.zoom

        min_x = half_screen_width
        max_x = self.map_width - half_screen_width
        min_y = half_screen_height
        max_y = self.map_height - half_screen_height

        # Apply clamping
        self.cam.target.x = max(min_x, min(self.cam.target.x, max_x))
        self.cam.target.y = max(min_y, min(self.cam.target.y, max_y))

        # Apply camera shake if active
        if self.camera_shake_duration > 0:
            shake_offset_x = random.uniform(
                -self.camera_shake_intensity, self.camera_shake_intensity
            )
            shake_offset_y = random.uniform(
                -self.camera_shake_intensity, self.camera_shake_intensity
            )
            self.cam.offset.x += shake_offset_x
            self.cam.offset.y += shake_offset_y
            self.camera_shake_duration -= delta_time
        else:
            # Reset offset if shake is over to prevent drift
            self.cam.offset = pr.Vector2(SCREEN_WIDTH // 2, SCREEN_HEIGHT // 2)

    def trigger_shake(self, intensity: float, duration: float):
        """Triggers a camera shake effect."""
        self.camera_shake_intensity = intensity
        self.camera_shake_duration = duration

    def begin_mode(self):
        """Begins 2D camera mode."""
        pr.begin_mode_2d(self.cam)

    def end_mode(self):
        """Ends 2D camera mode."""
        pr.end_mode_2d()


# --- Utility Functions ---
def find_valid_position(
    center_x: float,
    center_y: float,
    search_radius: float,
    object_width: float,
    object_height: float,
    existing_objects: List[GameObject],
    map_width: int,
    map_height: int,
    avoid_passable_collision: bool = False,  # New parameter: if True, avoids collision with passable objects
    max_attempts: int = 100,
) -> Position:
    """
    Finds a random valid position within a given radius for any object, avoiding existing objects
    and map boundaries.
    If `avoid_passable_collision` is True, it will avoid collision with objects where `passable` is True.
    Returns the initial center if no valid position is found after max_attempts.
    """
    for _ in range(max_attempts):
        # Generate a random angle and distance within the circle
        angle = random.uniform(0, 2 * math.pi)
        distance = random.uniform(0, search_radius)

        # Calculate potential position (adjust for object's top-left corner)
        potential_x = center_x + distance * math.cos(angle) - object_width / 2
        potential_y = center_y + distance * math.sin(angle) - object_height / 2

        # Create a temporary rectangle for collision checking
        temp_object_rect = pr.Rectangle(
            potential_x, potential_y, object_width, object_height
        )

        is_valid = True
        # Check collision with existing objects
        for obj in existing_objects:
            # If `avoid_passable_collision` is True, check collision with ALL objects.
            # Otherwise, only check collision with non-passable objects.
            if (
                avoid_passable_collision or not obj.passable
            ) and pr.check_collision_recs(temp_object_rect, obj.get_rect()):
                is_valid = False
                break

        # Check collision with map boundaries
        if (
            potential_x < 0
            or potential_x + object_width > map_width
            or potential_y < 0
            or potential_y + object_height > map_height
        ):
            is_valid = False

        if is_valid:
            return potential_x, potential_y

    # If no valid position found after max_attempts, return the initial center
    return center_x - object_width / 2, center_y - object_height / 2


# --- Map Generation ---
def generate_objects(num_objects: int, width: int, height: int) -> List[GameObject]:
    """Generates a list of random rock and bush objects."""
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


def create_map_data(
    map_id: int,
    num_obstacles: int,
    portal_configs: List[Dict],
    map_width: int,
    map_height: int,
) -> Dict[str, List[GameObject]]:
    """
    Generates map obstacles and safely places portals based on configurations.
    Portals are placed to avoid collision with all obstacles.
    """
    obstacles = generate_objects(num_obstacles, map_width, map_height)
    portals = []

    for i, config in enumerate(portal_configs):
        # Find a safe position for the current portal
        # Start search from a random point within the map
        initial_center_x = random.uniform(0, map_width)
        initial_center_y = random.uniform(0, map_height)

        portal_x, portal_y = find_valid_position(
            initial_center_x,
            initial_center_y,
            max(map_width, map_height),  # Search across the whole map if needed
            TILE_SIZE,
            TILE_SIZE,
            obstacles
            + portals,  # Check against existing obstacles AND already placed portals
            map_width,
            map_height,
            avoid_passable_collision=True,  # Portals must not collide with any object
        )
        portals.append(
            Portal(
                x=portal_x,
                y=portal_y,
                width=TILE_SIZE,
                height=TILE_SIZE,
                dest_map=config["dest_map"],
                dest_portal_index=config["dest_portal_index"],
                spawn_radius=config.get("spawn_radius", 100.0),
            )
        )
    return {"portals": portals, "obstacles": obstacles}


# --- Game State ---
# Define portal configurations for each map
portal_configs_map1 = [
    {
        "dest_map": 2,
        "dest_portal_index": 0,
        "spawn_radius": 150.0,
    },  # Links to map 2, portal 0
    {
        "dest_map": 1,
        "dest_portal_index": 0,
        "spawn_radius": 100.0,
    },  # Links to map 1, portal 0
]

portal_configs_map2 = [
    {
        "dest_map": 1,
        "dest_portal_index": 0,
        "spawn_radius": 150.0,
    },  # Links to map 1, portal 0
    {
        "dest_map": 2,
        "dest_portal_index": 0,
        "spawn_radius": 100.0,
    },  # Links to map 2, portal 0
]

# Generate map data using the new function
maps: Dict[int, Dict[str, List[GameObject]]] = {
    1: create_map_data(1, 200, portal_configs_map1, MAP_WIDTH, MAP_HEIGHT),
    2: create_map_data(2, 200, portal_configs_map2, MAP_WIDTH, MAP_HEIGHT),
}

current_map_id = 1
# Player initial position also needs to be safe
player_initial_x, player_initial_y = find_valid_position(
    MAP_WIDTH / 2,
    MAP_HEIGHT / 2,  # Start search from map center
    min(MAP_WIDTH, MAP_HEIGHT) / 4,  # Search radius for player initial spawn
    TILE_SIZE,
    TILE_SIZE,
    maps[current_map_id]["obstacles"]
    + maps[current_map_id]["portals"],  # Check against all objects
    MAP_WIDTH,
    MAP_HEIGHT,
    avoid_passable_collision=False,  # Player can spawn on passable objects (bushes)
)
player = Player(
    x=player_initial_x, y=player_initial_y, width=TILE_SIZE, height=TILE_SIZE
)
camera = GameCamera(player, MAP_WIDTH, MAP_HEIGHT)

collided_portal_info: str = "None"
portal_hold_time_display: float = 0.0


# --- Main Game Loop ---
def main():
    global current_map_id, collided_portal_info, portal_hold_time_display
    pr.init_window(SCREEN_WIDTH, SCREEN_HEIGHT, b"RPG Map Portal System with AOI")
    pr.set_target_fps(60)

    while not pr.window_should_close():
        delta_time = pr.get_frame_time()
        camera.update(delta_time)

        player_center_x, player_center_y = player.get_center()

        active_obstacles = []
        active_portals = []
        rendered_object_count = 0

        # Filter objects within AOI for rendering and collision
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
            if distance_sq < AOI_RADIUS**2:
                active_portals.append(portal)

        player.update(delta_time, active_obstacles)

        collided_portal_info = "None"
        portal_hold_time_display = 0.0

        for portal in active_portals:
            if pr.check_collision_recs(player.get_rect(), portal.get_rect()):
                # Get the destination portal object
                dest_map_id = portal.dest_map
                dest_portal_idx = portal.dest_portal_index

                # Ensure the destination portal index is valid
                if dest_portal_idx < len(maps[dest_map_id]["portals"]):
                    destination_portal = maps[dest_map_id]["portals"][dest_portal_idx]
                    collided_portal_info = f"Portal to Map {dest_map_id}, Portal {dest_portal_idx} ({int(destination_portal.x)},{int(destination_portal.y)})"
                else:
                    collided_portal_info = f"Portal link error: Dest portal {dest_portal_idx} not found in Map {dest_map_id}"
                    destination_portal = None  # Indicate invalid destination

                portal_hold_time_display = player.stationary_time

                if player.stationary_time >= TELEPORT_HOLD_TIME and destination_portal:
                    # Find a valid spawn position in the destination map for the player
                    new_map_obstacles = maps[dest_map_id]["obstacles"]
                    # Player can move over bushes, so avoid_passable_collision is False here
                    spawn_x, spawn_y = find_valid_position(
                        destination_portal.get_center()[
                            0
                        ],  # Use destination portal's center
                        destination_portal.get_center()[
                            1
                        ],  # Use destination portal's center
                        destination_portal.spawn_radius,  # Use destination portal's spawn radius
                        player.width,
                        player.height,
                        new_map_obstacles,
                        MAP_WIDTH,  # Pass map dimensions for boundary check
                        MAP_HEIGHT,  # Pass map dimensions for boundary check
                        avoid_passable_collision=False,  # Player can spawn on passable objects (bushes)
                    )
                    current_map_id = dest_map_id
                    player.x, player.y = spawn_x, spawn_y
                    player.stationary_time = 0.0
                    camera.trigger_shake(5.0, 0.2)
                    break  # Exit loop after successful teleport

        pr.begin_drawing()
        pr.clear_background(pr.RAYWHITE)
        camera.begin_mode()

        # Draw obstacles first (both passable and non-passable)
        for obj in active_obstacles:
            pr.draw_rectangle_rec(obj.get_rect(), obj.color)
            rendered_object_count += 1

        # Draw portals after obstacles so they appear on top
        for portal in active_portals:
            pr.draw_rectangle_rec(portal.get_rect(), portal.color)
            rendered_object_count += 1
            # Draw spawn radius for visual differentiation
            pr.draw_circle_lines(
                int(portal.get_center()[0]),
                int(portal.get_center()[1]),
                int(portal.spawn_radius),
                pr.RED,  # Use a distinct color for the radius
            )

        # Draw player last among game world objects
        pr.draw_rectangle_rec(player.get_rect(), player.color)

        pr.draw_circle_lines(
            int(player_center_x), int(player_center_y), AOI_RADIUS, pr.BLUE
        )

        pr.draw_rectangle_lines(0, 0, MAP_WIDTH, MAP_HEIGHT, pr.RED)

        camera.end_mode()

        # --- UI ---
        pr.draw_text(f"Map: {current_map_id}", 10, 10, 20, pr.DARKGRAY)
        pr.draw_text(
            f"Rendered objects: {rendered_object_count}", 10, 35, 20, pr.DARKGRAY
        )
        kb_usage = rendered_object_count * 16 / 1024
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
