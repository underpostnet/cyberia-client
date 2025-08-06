import pyray as pr
import random
from dataclasses import dataclass, field
from typing import Tuple, List, Optional
import math
from warnings import warn
import heapq
from collections import deque

# --- Constants ---
SCREEN_WIDTH = 800
SCREEN_HEIGHT = 600
TILE_SIZE = 32
TELEPORT_HOLD_TIME = 2.0
MAP_WIDTH = 1600
MAP_HEIGHT = 1200
AOI_RADIUS = 300.0
OBSTACLE_INFLATION_PADDING = 5
DEBUG_DRAW_GRID = False

Position = Tuple[float, float]
GridPosition = Tuple[int, int]


# --- Pathfinding ---
class Node:
    """A node for A* Pathfinding."""

    def __init__(self, parent: Optional["Node"], position: GridPosition):
        self.parent = parent
        self.position = position
        self.g = 0
        self.h = 0
        self.f = 0

    def __eq__(self, other):
        return self.position == other.position

    def __lt__(self, other):
        return self.f < other.f

    def __gt__(self, other):
        return self.f > other.f


class AStarPathfinder:
    """Encapsulates A* pathfinding logic."""

    def __init__(self, maze: List[List[int]]):
        self.maze = maze
        self.rows = len(maze)
        self.cols = len(maze[0])
        self.adjacent_squares = [(0, -1), (0, 1), (-1, 0), (1, 0)]  # Cardinal

    def _reconstruct_path(self, current_node: Node) -> List[GridPosition]:
        """Reconstructs the path from end node to start node."""
        path = []
        current = current_node
        while current:
            path.append(current.position)
            current = current.parent
        return path[::-1]

    def find_path(
        self, start: GridPosition, end: GridPosition, allow_diagonal: bool = False
    ) -> Optional[List[GridPosition]]:
        """
        Finds a path from start to end in the maze.
        Returns a list of grid positions or None if no path.
        """
        if not (0 <= start[0] < self.rows and 0 <= start[1] < self.cols):
            warn(f"A* start position {start} is out of bounds.")
            return None
        if not (0 <= end[0] < self.rows and 0 <= end[1] < self.cols):
            warn(f"A* end position {end} is out of bounds.")
            return None

        start_node = Node(None, start)
        end_node = Node(None, end)

        open_list: List[Node] = []
        closed_list: List[Node] = []
        heapq.heappush(open_list, start_node)

        max_iterations = self.rows * self.cols * 2
        outer_iterations = 0
        closest_node_to_end = start_node

        directions = list(self.adjacent_squares)
        if allow_diagonal:
            directions.extend([(-1, -1), (-1, 1), (1, -1), (1, 1)])

        while open_list:
            outer_iterations += 1
            if outer_iterations > max_iterations:
                warn("A* pathfinding exceeded max iterations. Returning closest path.")
                return self._reconstruct_path(closest_node_to_end)

            current_node = heapq.heappop(open_list)
            closed_list.append(current_node)

            if current_node.h < closest_node_to_end.h:
                closest_node_to_end = current_node

            if current_node == end_node:
                return self._reconstruct_path(current_node)

            for dr, dc in directions:
                node_position = (
                    current_node.position[0] + dr,
                    current_node.position[1] + dc,
                )

                if not (
                    0 <= node_position[0] < self.rows
                    and 0 <= node_position[1] < self.cols
                ):
                    continue
                if self.maze[node_position[0]][node_position[1]] != 0:
                    continue

                new_node = Node(current_node, node_position)

                if new_node in closed_list:
                    continue

                new_node.g = current_node.g + 1
                new_node.h = ((new_node.position[0] - end_node.position[0]) ** 2) + (
                    (new_node.position[1] - end_node.position[1]) ** 2
                )
                new_node.f = new_node.g + new_node.h

                found_in_open = False
                for open_node in open_list:
                    if new_node.position == open_node.position:
                        found_in_open = True
                        if new_node.g < open_node.g:
                            open_node.g = new_node.g
                            open_node.f = new_node.f
                            open_node.parent = new_node.parent
                            heapq.heapify(open_list)
                        break

                if not found_in_open:
                    heapq.heappush(open_list, new_node)

        warn(
            "Couldn't get a path to destination. Returning path to closest reachable node."
        )
        return self._reconstruct_path(closest_node_to_end)


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
    target_world_pos: Optional[pr.Vector2] = None
    path: List[GridPosition] = field(default_factory=list)
    current_path_index: int = 0

    def __post_init__(self):
        self.prev_pos = (self.x, self.y)

    def update(self, delta_time: float, obstacles: List[GameObject]):
        """Updates player position and handles collisions."""
        self.prev_pos = (self.x, self.y)

        if self.path and self.current_path_index < len(self.path):
            next_grid_pos = self.path[self.current_path_index]
            target_x, target_y = grid_to_world(next_grid_pos)

            player_center_x, player_center_y = self.get_center()
            dx = target_x - player_center_x
            dy = target_y - player_center_y
            distance = math.sqrt(dx**2 + dy**2)

            arrival_threshold = 5.0

            if distance > arrival_threshold:
                move_x = dx / distance * self.speed * delta_time
                move_y = dy / distance * self.speed * delta_time

                new_x = self.x + move_x
                new_y = self.y + move_y
            else:
                self.x, self.y = target_x - self.width / 2, target_y - self.height / 2
                self.current_path_index += 1
                if self.current_path_index >= len(self.path):
                    self.path = []
                    self.target_world_pos = None
                    self.stationary_time = 0.0
                return

            new_rect = pr.Rectangle(new_x, new_y, self.width, self.height)
            can_move = True
            for obj in obstacles:
                if not obj.passable and pr.check_collision_recs(
                    new_rect, obj.get_rect()
                ):
                    can_move = False
                    warn("Direct collision detected, stopping player movement.")
                    self.path = []
                    self.target_world_pos = None
                    self.stationary_time = 0.0
                    break

            if can_move:
                self.x, self.y = new_x, new_y
                self.stationary_time = 0.0
            else:
                self.stationary_time += delta_time
        else:
            self.stationary_time += delta_time


@dataclass
class Portal(GameObject):
    dest_map: int
    dest_portal_index: int
    spawn_radius: float = 100.0
    passable: bool = field(init=False, default=True)
    color: pr.Color = field(init=False, default_factory=lambda: pr.GREEN)


@dataclass
class Rock(GameObject):
    passable: bool = field(init=False, default=False)
    color: pr.Color = field(default_factory=lambda: pr.BROWN)

    def __post_init__(self):
        self.width = TILE_SIZE
        self.height = TILE_SIZE


@dataclass
class Bush(GameObject):
    passable: bool = field(init=False, default=True)
    color: pr.Color = field(default_factory=lambda: pr.DARKGREEN)

    def __post_init__(self):
        self.width = TILE_SIZE
        self.height = TILE_SIZE


@dataclass
class ClickEffect:
    x: float
    y: float
    timer: float = 0.5
    radius: float = 10.0
    color: pr.Color = pr.YELLOW

    def update(self, delta_time: float) -> bool:
        """Updates the effect's timer. Returns True if the effect is finished."""
        self.timer -= delta_time
        return self.timer <= 0

    def draw(self, camera_zoom: float):
        """Draws the click effect, fading out over time."""
        alpha = int(255 * (self.timer / 0.5))  # Alpha value from 0-255

        # Robustly determine color components
        r, g, b = 255, 255, 0  # Default to yellow
        if (
            hasattr(self.color, "r")
            and hasattr(self.color, "g")
            and hasattr(self.color, "b")
        ):
            # Assume it's a PyRay Color-like object
            r, g, b = self.color.r, self.color.g, self.color.b
        elif isinstance(self.color, tuple) and len(self.color) >= 3:
            # Assume it's an RGB or RGBA tuple
            r, g, b = self.color[0], self.color[1], self.color[2]
        else:
            warn(
                f"ClickEffect color is not a valid PyRay Color or RGB/RGBA tuple: {self.color}. Defaulting to yellow."
            )

        current_color = pr.Color(r, g, b, alpha)

        # Scale radius with camera zoom for consistent visual size
        pr.draw_circle_v(
            pr.Vector2(self.x, self.y), self.radius / camera_zoom, current_color
        )


# --- Camera ---
class GameCamera:
    """Manages the game camera."""

    def __init__(self, target: GameObject, map_width: int, map_height: int):
        self.target = target
        self.map_width = map_width
        self.map_height = map_height
        self.cam = pr.Camera2D()
        self.cam.offset = pr.Vector2(SCREEN_WIDTH // 2, SCREEN_HEIGHT // 2)
        self.cam.zoom = 1.0
        self.cam.target = pr.Vector2(
            self.target.x + self.target.width / 2,
            self.target.y + self.target.height / 2,
        )
        self.CAMERA_SMOOTHNESS = 0.05
        self.camera_shake_intensity = 0.0
        self.camera_shake_duration = 0.0

    def update(self, delta_time: float):
        """Updates camera position, applies clamping and shake."""
        desired_target_x = self.target.x + self.target.width / 2
        desired_target_y = self.target.y + self.target.height / 2

        self.cam.target.x = pr.lerp(
            self.cam.target.x, desired_target_x, self.CAMERA_SMOOTHNESS
        )
        self.cam.target.y = pr.lerp(
            self.cam.target.y, desired_target_y, self.CAMERA_SMOOTHNESS
        )

        half_screen_width = SCREEN_WIDTH / 2 / self.cam.zoom
        half_screen_height = SCREEN_HEIGHT / 2 / self.cam.zoom

        min_x = half_screen_width
        max_x = self.map_width - half_screen_width
        min_y = half_screen_height
        max_y = self.map_height - half_screen_height

        self.cam.target.x = max(min_x, min(self.cam.target.x, max_x))
        self.cam.target.y = max(min_y, min(self.cam.target.y, max_y))

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


# --- Coordinate Conversion Functions ---
def world_to_grid(world_x: float, world_y: float) -> GridPosition:
    """Converts world coordinates to grid (row, col) coordinates."""
    return int(world_y // TILE_SIZE), int(world_x // TILE_SIZE)


def grid_to_world(grid_pos: GridPosition) -> Position:
    """Converts grid (row, col) coordinates to world (x, y) coordinates (center of tile)."""
    return (
        grid_pos[1] * TILE_SIZE + TILE_SIZE / 2,
        grid_pos[0] * TILE_SIZE + TILE_SIZE / 2,
    )


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
    avoid_passable_collision: bool = False,
    navigation_grid: Optional[List[List[int]]] = None,
    max_attempts: int = 100,
) -> Position:
    """
    Finds a random valid position within a given radius for an object, avoiding existing objects
    and map boundaries.
    """
    initial_x = center_x - object_width / 2
    initial_y = center_y - object_height / 2

    for _ in range(max_attempts):
        angle = random.uniform(0, 2 * math.pi)
        distance = random.uniform(0, search_radius)

        potential_x = center_x + distance * math.cos(angle) - object_width / 2
        potential_y = center_y + distance * math.sin(angle) - object_height / 2

        temp_object_rect = pr.Rectangle(
            potential_x, potential_y, object_width, object_height
        )

        is_valid = True
        for obj in existing_objects:
            if (
                avoid_passable_collision or not obj.passable
            ) and pr.check_collision_recs(temp_object_rect, obj.get_rect()):
                is_valid = False
                break

        if not (
            0 <= potential_x < map_width - object_width
            and 0 <= potential_y < map_height - object_height
        ):
            is_valid = False

        if is_valid and navigation_grid:
            grid_row, grid_col = world_to_grid(
                potential_x + object_width / 2, potential_y + object_height / 2
            )
            if (
                not (
                    0 <= grid_row < len(navigation_grid)
                    and 0 <= grid_col < len(navigation_grid[0])
                )
                or navigation_grid[grid_row][grid_col] == 1
            ):
                is_valid = False

        if is_valid:
            return potential_x, potential_y

    return initial_x, initial_y


def find_closest_walkable_grid_pos(
    start_grid_pos: GridPosition,
    grid: List[List[int]],
    max_search_radius_tiles: int = 10,
) -> Optional[GridPosition]:
    """
    Performs a BFS to find the closest walkable (0) grid position.
    """
    grid_rows, grid_cols = len(grid), len(grid[0])
    q = deque([(start_grid_pos, 0)])
    visited = {start_grid_pos}
    directions = [(0, 1), (0, -1), (1, 0), (-1, 0), (1, 1), (1, -1), (-1, 1), (-1, -1)]

    while q:
        current_pos, dist = q.popleft()
        if grid[current_pos[0]][current_pos[1]] == 0:
            return current_pos
        if dist >= max_search_radius_tiles:
            continue

        for dr, dc in directions:
            neighbor_row, neighbor_col = current_pos[0] + dr, current_pos[1] + dc
            neighbor_pos = (neighbor_row, neighbor_col)

            if (
                0 <= neighbor_row < grid_rows
                and 0 <= neighbor_col < grid_cols
                and neighbor_pos not in visited
            ):
                visited.add(neighbor_pos)
                q.append((neighbor_pos, dist + 1))
    return None


def check_line_of_sight(
    start_world_x: float,
    start_world_y: float,
    end_world_x: float,
    end_world_y: float,
    grid: List[List[int]],
) -> bool:
    """Checks for clear line of sight between two world points on the grid."""
    grid_rows, grid_cols = len(grid), len(grid[0])
    start_row, start_col = world_to_grid(start_world_x, start_world_y)
    end_row, end_col = world_to_grid(end_world_x, end_world_y)

    dx = abs(end_col - start_col)
    dy = abs(end_row - start_row)
    sx = 1 if start_col < end_col else -1
    sy = 1 if start_row < end_row else -1
    err = dx - dy

    current_col, current_row = start_col, start_row

    while True:
        if (
            not (0 <= current_row < grid_rows and 0 <= current_col < grid_cols)
            or grid[current_row][current_col] == 1
        ):
            return False
        if current_col == end_col and current_row == end_row:
            break

        e2 = 2 * err
        if e2 > -dy:
            err -= dy
            current_col += sx
        if e2 < dx:
            err += dx
            current_row += sy
    return True


def smooth_path(
    path: List[GridPosition],
    grid: List[List[int]],
) -> List[GridPosition]:
    """Applies line-of-sight path smoothing."""
    if len(path) < 3:
        return path

    smoothed_path = [path[0]]
    i = 0
    while i < len(path) - 1:
        j = i + 1
        last_valid_j = j
        while j < len(path):
            start_world_x, start_world_y = grid_to_world(smoothed_path[-1])
            end_world_x, end_world_y = grid_to_world(path[j])

            if check_line_of_sight(
                start_world_x, start_world_y, end_world_x, end_world_y, grid
            ):
                last_valid_j = j
                j += 1
            else:
                break
        smoothed_path.append(path[last_valid_j])
        i = last_valid_j
    return smoothed_path


class NavigationGrid:
    """Manages the game's navigation grid."""

    def __init__(
        self, map_width: int, map_height: int, player_dimensions: Tuple[float, float]
    ):
        self.map_width = map_width
        self.map_height = map_height
        self.player_width, self.player_height = player_dimensions
        self.grid_rows = math.ceil(map_height / TILE_SIZE)
        self.grid_cols = math.ceil(map_width / TILE_SIZE)
        self.grid: List[List[int]] = []

    def create_grid(self, obstacles: List[GameObject]):
        """Creates the 2D grid for A* pathfinding."""
        self.grid = [[0 for _ in range(self.grid_cols)] for _ in range(self.grid_rows)]

        player_half_width = self.player_width / 2
        player_half_height = self.player_height / 2

        for obj in obstacles:
            if not obj.passable:
                inflated_x = obj.x - player_half_width - OBSTACLE_INFLATION_PADDING
                inflated_y = obj.y - player_half_height - OBSTACLE_INFLATION_PADDING
                inflated_width = (
                    obj.width + self.player_width + 2 * OBSTACLE_INFLATION_PADDING
                )
                inflated_height = (
                    obj.height + self.player_height + 2 * OBSTACLE_INFLATION_PADDING
                )

                start_row, start_col = world_to_grid(inflated_x, inflated_y)
                end_row, end_col = world_to_grid(
                    inflated_x + inflated_width - 1, inflated_y + inflated_height - 1
                )

                start_row = max(0, start_row)
                start_col = max(0, start_col)
                end_row = min(self.grid_rows - 1, end_row)
                end_col = min(self.grid_cols - 1, end_col)

                for r in range(start_row, end_row + 1):
                    for c in range(start_col, end_col + 1):
                        self.grid[r][c] = 1

    def get_grid(self) -> List[List[int]]:
        return self.grid


# --- Map Management ---
class Map:
    """Represents a single game map."""

    def __init__(
        self,
        map_id: int,
        num_obstacles: int,
        portal_configs: List[dict],
        player_dimensions: Tuple[float, float],
    ):
        self.map_id = map_id
        self.obstacles = self._generate_objects(num_obstacles)
        self.portals = self._create_portals(portal_configs, player_dimensions)
        self.navigation_grid_manager = NavigationGrid(
            MAP_WIDTH, MAP_HEIGHT, player_dimensions
        )
        self.navigation_grid_manager.create_grid(self.obstacles + self.portals)

    def _generate_objects(self, num_objects: int) -> List[GameObject]:
        """Generates random rock and bush objects."""
        objs = []
        for _ in range(num_objects):
            x, y = random.randint(0, MAP_WIDTH - TILE_SIZE), random.randint(
                0, MAP_HEIGHT - TILE_SIZE
            )
            if random.random() < 0.5:
                objs.append(
                    Rock(x=float(x), y=float(y), width=TILE_SIZE, height=TILE_SIZE)
                )
            else:
                objs.append(
                    Bush(x=float(x), y=float(y), width=TILE_SIZE, height=TILE_SIZE)
                )
        return objs

    def _create_portals(
        self, portal_configs: List[dict], player_dimensions: Tuple[float, float]
    ) -> List[Portal]:
        """Safely places portals based on configurations."""
        portals = []
        for config in portal_configs:
            initial_center_x = random.uniform(0, MAP_WIDTH)
            initial_center_y = random.uniform(0, MAP_HEIGHT)

            portal_x, portal_y = find_valid_position(
                initial_center_x,
                initial_center_y,
                max(MAP_WIDTH, MAP_HEIGHT),
                TILE_SIZE,
                TILE_SIZE,
                self.obstacles + portals,
                MAP_WIDTH,
                MAP_HEIGHT,
                avoid_passable_collision=True,
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
        return portals

    def get_active_objects(
        self, player_center: Position
    ) -> Tuple[List[GameObject], List[Portal]]:
        """Returns objects within the Area of Interest (AOI)."""
        active_obstacles = []
        active_portals = []
        player_center_x, player_center_y = player_center

        for obj in self.obstacles:
            obj_center_x, obj_center_y = obj.get_center()
            distance_sq = (player_center_x - obj_center_x) ** 2 + (
                player_center_y - obj_center_y
            ) ** 2
            if distance_sq < AOI_RADIUS**2:
                active_obstacles.append(obj)

        for portal in self.portals:
            portal_center_x, portal_center_y = portal.get_center()
            distance_sq = (player_center_x - portal_center_x) ** 2 + (
                player_center_y - portal_center_y
            ) ** 2
            if distance_sq < AOI_RADIUS**2:
                active_portals.append(portal)
        return active_obstacles, active_portals

    def get_navigation_grid(self) -> List[List[int]]:
        return self.navigation_grid_manager.get_grid()


class GameState:
    """Manages the overall game state."""

    def __init__(self):
        self.portal_configs_map1 = [
            {"dest_map": 2, "dest_portal_index": 0, "spawn_radius": 150.0},
            {"dest_map": 1, "dest_portal_index": 0, "spawn_radius": 100.0},
        ]
        self.portal_configs_map2 = [
            {"dest_map": 1, "dest_portal_index": 0, "spawn_radius": 150.0},
            {"dest_map": 2, "dest_portal_index": 0, "spawn_radius": 100.0},
        ]

        self.player_dimensions = (TILE_SIZE, TILE_SIZE)
        self.maps = {
            1: Map(1, 200, self.portal_configs_map1, self.player_dimensions),
            2: Map(2, 200, self.portal_configs_map2, self.player_dimensions),
        }

        self.current_map_id = 1
        initial_spawn_grid = self.maps[self.current_map_id].get_navigation_grid()
        player_initial_x, player_initial_y = find_valid_position(
            MAP_WIDTH / 2,
            MAP_HEIGHT / 2,
            min(MAP_WIDTH, MAP_HEIGHT) / 4,
            self.player_dimensions[0],
            self.player_dimensions[1],
            self.maps[self.current_map_id].obstacles
            + self.maps[self.current_map_id].portals,
            MAP_WIDTH,
            MAP_HEIGHT,
            avoid_passable_collision=False,
            navigation_grid=initial_spawn_grid,
        )
        self.player = Player(
            x=player_initial_x,
            y=player_initial_y,
            width=self.player_dimensions[0],
            height=self.player_dimensions[1],
        )
        self.camera = GameCamera(self.player, MAP_WIDTH, MAP_HEIGHT)

        self.collided_portal_info: str = "None"
        self.portal_hold_time_display: float = 0.0
        self.active_click_effects: List[ClickEffect] = []
        self.debug_draw_grid = False

    def get_current_map(self) -> Map:
        return self.maps[self.current_map_id]

    def handle_teleport(self, portal: Portal):
        """Handles player teleportation through a portal."""
        dest_map_id = portal.dest_map
        dest_portal_idx = portal.dest_portal_index
        destination_portal = None

        if dest_portal_idx < len(self.maps[dest_map_id].portals):
            destination_portal = self.maps[dest_map_id].portals[dest_portal_idx]
            self.collided_portal_info = f"Portal to Map {dest_map_id}, Portal {dest_portal_idx} ({int(destination_portal.x)},{int(destination_portal.y)})"
        else:
            self.collided_portal_info = f"Portal link error: Dest portal {dest_portal_idx} not found in Map {dest_map_id}"
            return

        self.portal_hold_time_display = self.player.stationary_time

        if self.player.stationary_time >= TELEPORT_HOLD_TIME and destination_portal:
            self.player.path = []
            self.player.target_world_pos = None
            self.player.current_path_index = 0

            new_map_obstacles = self.maps[dest_map_id].obstacles
            new_map_grid = self.maps[dest_map_id].get_navigation_grid()

            spawn_x, spawn_y = find_valid_position(
                destination_portal.get_center()[0],
                destination_portal.get_center()[1],
                destination_portal.spawn_radius,
                self.player.width,
                self.player.height,
                new_map_obstacles,
                MAP_WIDTH,
                MAP_HEIGHT,
                avoid_passable_collision=False,
                navigation_grid=new_map_grid,
            )
            self.current_map_id = dest_map_id
            self.player.x, self.player.y = spawn_x, spawn_y
            self.player.stationary_time = 0.0
            self.camera.trigger_shake(5.0, 0.2)


class GameRenderer:
    """Handles all drawing operations."""

    def __init__(self, game_state: GameState):
        self.game_state = game_state

    def draw_game_elements(
        self, active_obstacles: List[GameObject], active_portals: List[Portal]
    ):
        """Draws obstacles, portals, and player."""
        rendered_object_count = 0
        for obj in active_obstacles:
            pr.draw_rectangle_rec(obj.get_rect(), obj.color)
            rendered_object_count += 1

        for portal in active_portals:
            pr.draw_rectangle_rec(portal.get_rect(), portal.color)
            rendered_object_count += 1
            pr.draw_circle_lines(
                int(portal.get_center()[0]),
                int(portal.get_center()[1]),
                int(portal.spawn_radius),
                pr.RED,
            )

        pr.draw_rectangle_rec(
            self.game_state.player.get_rect(), self.game_state.player.color
        )
        return rendered_object_count

    def draw_path(self):
        """Draws the player's path."""
        player = self.game_state.player
        if player.path:
            for i, grid_pos in enumerate(player.path):
                world_x, world_y = grid_to_world(grid_pos)
                if i == 0 and player.current_path_index == 0:
                    pr.draw_line_ex(
                        pr.Vector2(
                            player.x + player.width / 2, player.y + player.height / 2
                        ),
                        pr.Vector2(world_x, world_y),
                        2,
                        pr.YELLOW,
                    )
                elif i > 0:
                    prev_world_x, prev_world_y = grid_to_world(player.path[i - 1])
                    pr.draw_line_ex(
                        pr.Vector2(prev_world_x, prev_world_y),
                        pr.Vector2(world_x, world_y),
                        2,
                        pr.YELLOW,
                    )
                pr.draw_circle(
                    int(world_x),
                    int(world_y),
                    5,
                    pr.ORANGE if i == player.current_path_index else pr.YELLOW,
                )

    def draw_debug_grid(self):
        """Draws the navigation grid for debugging."""
        if self.game_state.debug_draw_grid:
            grid = self.game_state.get_current_map().get_navigation_grid()
            for r_idx, row in enumerate(grid):
                for c_idx, cell in enumerate(row):
                    if cell == 1:
                        grid_world_x = c_idx * TILE_SIZE
                        grid_world_y = r_idx * TILE_SIZE
                        pr.draw_rectangle(
                            int(grid_world_x),
                            int(grid_world_y),
                            TILE_SIZE,
                            TILE_SIZE,
                            pr.fade(pr.RED, 0.3),
                        )

    def draw_click_effects(self, delta_time: float):
        """Updates and draws click effects."""
        for effect in list(self.game_state.active_click_effects):
            if effect.update(delta_time):
                self.game_state.active_click_effects.remove(effect)
            else:
                effect.draw(self.game_state.camera.cam.zoom)

    def draw_aoi_and_map_boundary(self):
        """Draws AOI circle and map boundary."""
        player_center_x, player_center_y = self.game_state.player.get_center()
        pr.draw_circle_lines(
            int(player_center_x), int(player_center_y), AOI_RADIUS, pr.BLUE
        )
        pr.draw_rectangle_lines(0, 0, MAP_WIDTH, MAP_HEIGHT, pr.RED)

    def draw_ui(self, rendered_object_count: int):
        """Draws UI elements."""
        pr.draw_text(f"Map: {self.game_state.current_map_id}", 10, 10, 20, pr.DARKGRAY)
        pr.draw_text(
            f"Rendered objects: {rendered_object_count}", 10, 35, 20, pr.DARKGRAY
        )
        kb_usage = rendered_object_count * 16 / 1024
        pr.draw_text(f"Memory (KB, rendered): {kb_usage:.1f}", 10, 60, 20, pr.DARKGRAY)

        if self.game_state.collided_portal_info != "None":
            pr.draw_text(
                f"Hold for {TELEPORT_HOLD_TIME:.1f}s: {self.game_state.portal_hold_time_display:.1f}s",
                10,
                85,
                20,
                pr.BLUE,
            )
        else:
            pr.draw_text("Hold for portal: Not colliding", 10, 85, 20, pr.DARKGRAY)

        player_grid = world_to_grid(self.game_state.player.x, self.game_state.player.y)
        pr.draw_text(f"Player Grid: {player_grid}", 10, 110, 20, pr.DARKGRAY)
        if self.game_state.player.path:
            pr.draw_text(
                f"Target Grid: {self.game_state.player.path[-1]}",
                10,
                135,
                20,
                pr.DARKGRAY,
            )
        else:
            pr.draw_text("Target Grid: None", 10, 135, 20, pr.DARKGRAY)

        self._draw_minimap()

    def _draw_minimap(self):
        """Draws the minimap."""
        minimap_size = 200
        mm_x, mm_y = SCREEN_WIDTH - minimap_size - 10, SCREEN_HEIGHT - minimap_size - 10
        pr.draw_rectangle(mm_x, mm_y, minimap_size, minimap_size, pr.LIGHTGRAY)
        pr.draw_rectangle_lines(mm_x, mm_y, minimap_size, minimap_size, pr.DARKGRAY)
        scale_x, scale_y = minimap_size / MAP_WIDTH, minimap_size / MAP_HEIGHT

        current_map = self.game_state.get_current_map()
        for obj in current_map.obstacles:
            pr.draw_rectangle(
                mm_x + int(obj.x * scale_x),
                mm_y + int(obj.y * scale_y),
                2,
                2,
                obj.color,
            )
        for portal in current_map.portals:
            pr.draw_rectangle(
                mm_x + int(portal.x * scale_x),
                mm_y + int(portal.y * scale_y),
                4,
                4,
                portal.color,
            )
        pr.draw_circle(
            mm_x + int(self.game_state.player.x * scale_x),
            mm_y + int(self.game_state.player.y * scale_y),
            3,
            self.game_state.player.color,
        )


class Game:
    """Main game class to encapsulate game loop and logic."""

    def __init__(self):
        pr.init_window(SCREEN_WIDTH, SCREEN_HEIGHT, b"RPG Map Portal System with AOI")
        pr.set_target_fps(60)
        self.game_state = GameState()
        self.game_renderer = GameRenderer(self.game_state)
        self.pathfinder = AStarPathfinder(
            self.game_state.get_current_map().get_navigation_grid()
        )

    def _handle_input(self):
        """Processes user input."""
        if pr.is_key_pressed(pr.KEY_G):
            self.game_state.debug_draw_grid = not self.game_state.debug_draw_grid

        if pr.is_mouse_button_pressed(pr.MOUSE_LEFT_BUTTON):
            mouse_world_pos = pr.get_screen_to_world_2d(
                pr.get_mouse_position(), self.game_state.camera.cam
            )
            self.game_state.active_click_effects.append(
                ClickEffect(x=mouse_world_pos.x, y=mouse_world_pos.y)
            )

            target_grid_pos = world_to_grid(mouse_world_pos.x, mouse_world_pos.y)
            player_center_grid_pos = world_to_grid(
                self.game_state.player.x + self.game_state.player.width / 2,
                self.game_state.player.y + self.game_state.player.height / 2,
            )

            current_grid = self.game_state.get_current_map().get_navigation_grid()
            grid_rows, grid_cols = len(current_grid), len(current_grid[0])

            path_start_grid_pos = player_center_grid_pos
            if not (
                0 <= player_center_grid_pos[0] < grid_rows
                and 0 <= player_center_grid_pos[1] < grid_cols
                and current_grid[player_center_grid_pos[0]][player_center_grid_pos[1]]
                == 0
            ):
                warn(
                    f"Player's current position {player_center_grid_pos} is an obstacle. Finding closest walkable start."
                )
                closest_valid_start = find_closest_walkable_grid_pos(
                    player_center_grid_pos, current_grid
                )
                if closest_valid_start:
                    path_start_grid_pos = closest_valid_start
                else:
                    warn(
                        f"Could not find a walkable start position near player {player_center_grid_pos}. Clearing path."
                    )
                    self.game_state.player.path = []
                    self.game_state.player.target_world_pos = None
                    return

            final_target_grid_pos = target_grid_pos
            if not (
                0 <= target_grid_pos[0] < grid_rows
                and 0 <= target_grid_pos[1] < grid_cols
                and current_grid[target_grid_pos[0]][target_grid_pos[1]] == 0
            ):
                warn(
                    f"Clicked location {target_grid_pos} is an obstacle. Finding closest walkable target."
                )
                closest_valid_target = find_closest_walkable_grid_pos(
                    target_grid_pos, current_grid
                )
                if closest_valid_target:
                    final_target_grid_pos = closest_valid_target
                else:
                    warn(
                        f"Could not find a walkable target position near click {target_grid_pos}. Clearing path."
                    )
                    self.game_state.player.path = []
                    self.game_state.player.target_world_pos = None
                    return

            # Update the pathfinder's maze for the current map
            self.pathfinder.maze = current_grid
            path = self.pathfinder.find_path(path_start_grid_pos, final_target_grid_pos)
            if path:
                smoothed_path = smooth_path(path, current_grid)
                self.game_state.player.path = smoothed_path[1:]
                self.game_state.player.current_path_index = 0
                self.game_state.player.target_world_pos = mouse_world_pos
            else:
                warn("No path found to (or near) clicked location.")
                self.game_state.player.path = []
                self.game_state.player.target_world_pos = None

    def _update_game_logic(self, delta_time: float):
        """Updates game logic including player movement and portal interactions."""
        self.game_state.camera.update(delta_time)

        current_map = self.game_state.get_current_map()
        active_obstacles, active_portals = current_map.get_active_objects(
            self.game_state.player.get_center()
        )

        self.game_state.player.update(delta_time, active_obstacles)

        self.game_state.collided_portal_info = "None"
        self.game_state.portal_hold_time_display = 0.0

        for portal in active_portals:
            if pr.check_collision_recs(
                self.game_state.player.get_rect(), portal.get_rect()
            ):
                self.game_state.handle_teleport(portal)
                break

    def run(self):
        """Runs the main game loop."""
        while not pr.window_should_close():
            delta_time = pr.get_frame_time()
            self._handle_input()
            self._update_game_logic(delta_time)

            pr.begin_drawing()
            pr.clear_background(pr.RAYWHITE)
            self.game_state.camera.begin_mode()

            current_map = self.game_state.get_current_map()
            active_obstacles, active_portals = current_map.get_active_objects(
                self.game_state.player.get_center()
            )

            rendered_object_count = self.game_renderer.draw_game_elements(
                active_obstacles, active_portals
            )
            self.game_renderer.draw_path()
            self.game_renderer.draw_debug_grid()
            self.game_renderer.draw_aoi_and_map_boundary()
            self.game_state.camera.end_mode()

            self.game_renderer.draw_click_effects(delta_time)
            self.game_renderer.draw_ui(rendered_object_count)

            pr.end_drawing()

        pr.close_window()


if __name__ == "__main__":
    game = Game()
    game.run()
