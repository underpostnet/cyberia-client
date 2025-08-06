import pyray as pr
import random
from dataclasses import dataclass, field
from typing import Tuple, Dict, List, Optional
import math
from warnings import warn
import heapq
from collections import deque  # For BFS in find_closest_walkable_grid_pos

# --- Constants ---
SCREEN_WIDTH = 800
SCREEN_HEIGHT = 600
TILE_SIZE = 32
TELEPORT_HOLD_TIME = 2.0
MAP_WIDTH = 1600
MAP_HEIGHT = 1200
AOI_RADIUS = 300.0
# Pixels to expand obstacles by in the grid for pathfinding, relative to player's size
# This ensures the player's bounding box doesn't collide.
# A value of 0 means the path is for the center of a 1-tile wide player.
# A higher value means more clearance around obstacles.
OBSTACLE_INFLATION_PADDING = 5  # Increased for more clearance

DEBUG_DRAW_GRID = False  # Set to True to visualize the navigation grid

Position = Tuple[float, float]  # World coordinates (x, y)
GridPosition = Tuple[int, int]  # Grid coordinates (row, col)


# --- A* Pathfinding (from astar.py) ---
class Node:
    """
    A node class for A* Pathfinding algorithm.
    """

    def __init__(self, parent=None, position=None):
        self.parent = parent
        self.position = position  # (row, col)

        self.g = 0  # Cost from start to current node
        self.h = 0  # Heuristic cost from current node to end node
        self.f = 0  # Total cost (g + h)

    def __eq__(self, other):
        return self.position == other.position

    def __repr__(self):
        return f"{self.position} - g: {self.g} h: {self.h} f: {self.f}"

    # Define less than and greater than for heap queue operations
    def __lt__(self, other):
        return self.f < other.f

    def __gt__(self, other):
        return self.f > other.f


def return_path(current_node):
    """Reconstructs the path from the end node back to the start node."""
    path = []
    current = current_node
    while current is not None:
        path.append(current.position)
        current = current.parent
    return path[::-1]  # Return reversed path to get it from start to end


def astar(
    maze, start: GridPosition, end: GridPosition, allow_diagonal_movement=False
) -> Optional[List[GridPosition]]:
    """
    Returns a list of tuples (row, col) as a path from the given start to the given end in the given maze.
    If a full path cannot be found, it returns the path to the closest reachable node to the end.
    :param maze: A 2D list (grid) where 0 is walkable and 1 is an obstacle.
    :param start: A tuple (row, col) representing the start position.
    :param end: A tuple (row, col) representing the end position.
    :param allow_diagonal_movement: Boolean to allow diagonal movement.
    :return: A list of tuples (row, col) representing the path, or None if no path is found.
    """

    # Ensure start and end are within maze boundaries before checking for obstacles
    if not (0 <= start[0] < len(maze) and 0 <= start[1] < len(maze[0])):
        warn(f"A* start position {start} is out of bounds.")
        return None
    if not (0 <= end[0] < len(maze) and 0 <= end[1] < len(maze[0])):
        warn(f"A* end position {end} is out of bounds.")
        # If end is out of bounds, no path can be found to it.
        return None

    # Check if start is an obstacle - handled by calling function now
    # if maze[start[0]][start[1]] == 1:
    #     warn(f"A* start position {start} is an an obstacle.")
    #     return None
    # If end is an obstacle, A* will naturally find the closest walkable node.
    # No explicit change needed here, as the closest_node_to_end logic handles it.

    start_node = Node(None, start)
    start_node.g = start_node.h = start_node.f = 0
    end_node = Node(None, end)
    end_node.g = end_node.h = end_node.f = 0

    open_list = []  # Priority queue for nodes to be evaluated
    closed_list = []  # List of nodes already evaluated

    heapq.heapify(open_list)
    heapq.heappush(open_list, start_node)

    # Safety break for infinite loops, adjusted for grid size
    max_iterations = len(maze[0]) * len(maze) * 2
    outer_iterations = 0

    # Define possible movements (up, down, left, right)
    adjacent_squares = (
        (0, -1),  # Up
        (0, 1),  # Down
        (-1, 0),  # Left
        (1, 0),  # Right
    )
    if allow_diagonal_movement:
        # Add diagonal movements
        adjacent_squares += (
            (-1, -1),  # Up-Left
            (-1, 1),  # Down-Left
            (1, -1),  # Up-Right
            (1, 1),  # Down-Right
        )

    # Keep track of the closest node found to the end node
    closest_node_to_end = start_node

    while len(open_list) > 0:
        outer_iterations += 1
        if outer_iterations > max_iterations:
            warn(
                "A* pathfinding exceeded max iterations. Returning path to closest reachable node."
            )
            return return_path(closest_node_to_end)

        current_node = heapq.heappop(open_list)
        closed_list.append(current_node)

        # Update closest_node_to_end if current_node is closer
        # Use heuristic (h) as a measure of distance to end
        if current_node.h < closest_node_to_end.h:
            closest_node_to_end = current_node

        if current_node == end_node:
            return return_path(current_node)

        children = []
        for new_position in adjacent_squares:
            node_position = (
                current_node.position[0] + new_position[0],
                current_node.position[1] + new_position[1],
            )

            # Check if within maze boundaries
            if not (
                0 <= node_position[0] < len(maze)
                and 0 <= node_position[1] < len(maze[0])
            ):
                continue

            # Check if walkable terrain (0 means walkable)
            if maze[node_position[0]][node_position[1]] != 0:
                continue

            new_node = Node(current_node, node_position)
            children.append(new_node)

        for child in children:
            # Child is already in the closed list
            if child in closed_list:
                continue

            # Calculate f, g, and h values
            child.g = current_node.g + 1
            # Heuristic: Euclidean distance squared
            child.h = ((child.position[0] - end_node.position[0]) ** 2) + (
                (child.position[1] - end_node.position[1]) ** 2
            )
            child.f = child.g + child.h

            # Child is already in the open list and has a worse G score
            # This check is crucial for efficiency: if we found a better path to a node already in open_list, update it.
            # Otherwise, just add it.
            found_in_open = False
            for open_node in open_list:
                if child.position == open_node.position:
                    found_in_open = True
                    if child.g < open_node.g:  # If new path is better
                        open_node.g = child.g
                        open_node.f = child.f
                        open_node.parent = child.parent  # Update parent
                        heapq.heapify(open_list)  # Re-heapify after modification
                    break

            if not found_in_open:
                heapq.heappush(open_list, child)

    # If open_list is empty and end_node was not reached, return path to the closest node found
    warn(
        "Couldn't get a path to destination. Returning path to closest reachable node."
    )
    return return_path(closest_node_to_end)


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
        """Initializes the previous position after object creation."""
        self.prev_pos = (self.x, self.y)

    def update(
        self,
        delta_time: float,
        obstacles: List[GameObject],
        navigation_grid: List[List[int]],
    ):
        """
        Updates the player's position based on pathfinding and handles collisions.
        Collision with non-passable objects will stop the player.
        """
        self.prev_pos = (self.x, self.y)

        if self.path and self.current_path_index < len(self.path):
            next_grid_pos = self.path[self.current_path_index]
            target_x, target_y = grid_to_world(next_grid_pos)

            # Calculate dx and dy from the player's current center to the target tile's center
            player_center_x, player_center_y = self.get_center()
            dx = target_x - player_center_x
            dy = target_y - player_center_y
            distance = math.sqrt(dx**2 + dy**2)

            arrival_threshold = 5.0  # Tolerance for reaching the center of a tile

            if distance > arrival_threshold:
                move_x = dx / distance * self.speed * delta_time
                move_y = dy / distance * self.speed * delta_time

                new_x = self.x + move_x
                new_y = self.y + move_y
            else:
                # Arrived at the current path tile, move to the next
                self.x, self.y = (
                    target_x - self.width / 2,
                    target_y - self.height / 2,
                )  # Snap to top-left of tile
                self.current_path_index += 1
                if self.current_path_index >= len(self.path):
                    # Reached the end of the path
                    self.path = []
                    self.target_world_pos = None
                    self.stationary_time = 0.0
                return  # Skip collision check for this frame if just moved to next tile

            new_rect = pr.Rectangle(new_x, new_y, self.width, self.height)

            # Check for direct collision with non-passable objects
            # This is a final safeguard, the pathfinding grid should prevent most collisions
            can_move = True
            for obj in obstacles:
                if not obj.passable and pr.check_collision_recs(
                    new_rect, obj.get_rect()
                ):
                    can_move = False
                    warn("Direct collision detected, stopping player movement.")
                    self.path = []  # Clear path
                    self.target_world_pos = None
                    self.stationary_time = 0.0
                    break  # Stop checking other obstacles

            if can_move:
                self.x, self.y = new_x, new_y
                self.stationary_time = 0.0  # Reset stationary time if moving
            else:
                self.stationary_time += (
                    delta_time  # Player is trying to move but blocked
                )
        else:
            # If no path, player is stationary
            self.stationary_time += delta_time


@dataclass
class Portal(GameObject):
    dest_map: int
    dest_portal_index: int
    spawn_radius: float = 100.0

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


@dataclass
class ClickEffect:
    x: float
    y: float
    timer: float = 0.5  # Duration in seconds
    radius: float = 10.0
    # Changed default color initialization to direct assignment to pr.Color
    color: pr.Color = pr.YELLOW

    def update(self, delta_time: float) -> bool:
        """Updates the effect's timer. Returns True if the effect is finished."""
        self.timer -= delta_time
        return self.timer <= 0

    def draw(self, camera_zoom: float):
        """Draws the click effect, fading out over time."""
        alpha = int(255 * (self.timer / 0.5))  # Fade out effect

        # Defensive check and conversion for self.color
        if isinstance(self.color, tuple):
            # Assuming it's an RGBA tuple (r, g, b, a) or (r, g, b)
            if len(self.color) == 4:
                r, g, b, a_val = self.color
                current_color = pr.Color(r, g, b, alpha)  # Use provided alpha
            elif len(self.color) == 3:
                r, g, b = self.color
                current_color = pr.Color(r, g, b, alpha)  # Use provided alpha
            else:
                current_color = pr.Color(
                    255, 255, 0, alpha
                )  # Default yellow if tuple format is unexpected
        elif isinstance(self.color, pr.Color):
            current_color = pr.Color(self.color.r, self.color.g, self.color.b, alpha)
        else:
            current_color = pr.Color(
                255, 255, 0, alpha
            )  # Default yellow if type is unexpected

        # Scale radius with camera zoom for consistent visual size
        pr.draw_circle_v(
            pr.Vector2(self.x, self.y), self.radius / camera_zoom, current_color
        )


# --- Camera ---
class GameCamera:
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
        """Updates the camera's position, applies clamping and shake."""
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
    row = int(world_y // TILE_SIZE)
    col = int(world_x // TILE_SIZE)
    return row, col


def grid_to_world(grid_pos: GridPosition) -> Position:
    """Converts grid (row, col) coordinates to world (x, y) coordinates (center of tile)."""
    row, col = grid_pos
    world_x = col * TILE_SIZE + TILE_SIZE / 2
    world_y = row * TILE_SIZE + TILE_SIZE / 2
    return world_x, world_y


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
    navigation_grid: Optional[
        List[List[int]]
    ] = None,  # New: Optional navigation grid for A* walkability check
    max_attempts: int = 100,
) -> Position:
    """
    Finds a random valid position within a given radius for any object, avoiding existing objects
    and map boundaries.
    If `avoid_passable_collision` is True, it will avoid collision with objects where `passable` is True.
    If `navigation_grid` is provided, it also ensures the position is on a walkable tile (0).
    Returns the initial center if no valid position is found after max_attempts.
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

        if (
            potential_x < 0
            or potential_x + object_width > map_width
            or potential_y < 0
            or potential_y + object_height > map_height
        ):
            is_valid = False

        # Additional check for A* grid walkability if provided
        if is_valid and navigation_grid is not None:
            grid_row, grid_col = world_to_grid(
                potential_x + object_width / 2, potential_y + object_height / 2
            )  # Use center for grid check
            grid_rows = len(navigation_grid)
            grid_cols = len(navigation_grid[0])
            if (
                not (0 <= grid_row < grid_rows and 0 <= grid_col < grid_cols)
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
    Performs a BFS to find the closest walkable (0) grid position to the start_grid_pos.
    Returns the closest walkable grid position or None if none found within radius.
    """
    grid_rows = len(grid)
    grid_cols = len(grid[0])

    q = deque([(start_grid_pos, 0)])  # (position, distance)
    visited = {start_grid_pos}

    # Define possible movements (up, down, left, right, and diagonals)
    # Using all 8 directions for a more comprehensive search for closest point
    directions = [
        (0, 1),
        (0, -1),
        (1, 0),
        (-1, 0),  # Cardinal
        (1, 1),
        (1, -1),
        (-1, 1),
        (-1, -1),  # Diagonal
    ]

    while q:
        current_pos, dist = q.popleft()

        # If current position is walkable, return it
        if grid[current_pos[0]][current_pos[1]] == 0:
            return current_pos

        # If we exceed max search radius, stop searching from this branch
        if dist >= max_search_radius_tiles:
            continue

        # Explore neighbors
        for dr, dc in directions:
            neighbor_row, neighbor_col = current_pos[0] + dr, current_pos[1] + dc
            neighbor_pos = (neighbor_row, neighbor_col)

            # Check bounds and if already visited
            if (
                0 <= neighbor_row < grid_rows
                and 0 <= neighbor_col < grid_cols
                and neighbor_pos not in visited
            ):

                visited.add(neighbor_pos)
                q.append((neighbor_pos, dist + 1))

    return None  # No walkable tile found within the search radius


def check_line_of_sight(
    start_world_x: float,
    start_world_y: float,
    end_world_x: float,
    end_world_y: float,
    grid: List[List[int]],
    player_width: float,
    player_height: float,
) -> bool:
    """
    Checks if there is a clear line of sight between two world points on the grid,
    considering the player's dimensions.
    Assumes the grid is already inflated by player's half-dimensions.
    """
    grid_rows = len(grid)
    grid_cols = len(grid[0])

    # Convert world coordinates to grid coordinates for start and end points
    start_row, start_col = world_to_grid(start_world_x, start_world_y)
    end_row, end_col = world_to_grid(end_world_x, end_world_y)

    # Use Bresenham's line algorithm to iterate through grid cells
    dx = abs(end_col - start_col)
    dy = abs(end_row - start_row)
    sx = 1 if start_col < end_col else -1
    sy = 1 if start_row < end_row else -1
    err = dx - dy

    current_col, current_row = start_col, start_row

    while True:
        # Check if current grid cell is an obstacle (1)
        if (
            not (0 <= current_row < grid_rows and 0 <= current_col < grid_cols)
            or grid[current_row][current_col] == 1
        ):
            return False  # Collision with boundary or obstacle

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
    player_width: float,
    player_height: float,
) -> List[GridPosition]:
    """
    Applies simple line-of-sight path smoothing to remove redundant waypoints.
    """
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
                start_world_x,
                start_world_y,
                end_world_x,
                end_world_y,
                grid,
                player_width,
                player_height,
            ):
                last_valid_j = j
                j += 1
            else:
                break
        smoothed_path.append(path[last_valid_j])
        i = last_valid_j
    return smoothed_path


def create_navigation_grid(
    obstacles: List[GameObject],
    map_width: int,
    map_height: int,
    player_width: float,
    player_height: float,
) -> List[List[int]]:
    """
    Creates a 2D grid (matrix) for A* pathfinding.
    0 = walkable, 1 = obstacle.
    Non-passable GameObjects are marked as 1. Passable GameObjects (like bushes and portals) are 0.
    Obstacles are "inflated" by the player's half-dimensions plus a padding to ensure clearance.
    """
    grid_rows = math.ceil(map_height / TILE_SIZE)
    grid_cols = math.ceil(map_width / TILE_SIZE)

    grid = [[0 for _ in range(grid_cols)] for _ in range(grid_rows)]

    player_half_width = player_width / 2
    player_half_height = player_height / 2

    for obj in obstacles:
        if not obj.passable:  # Only non-passable objects are obstacles for A*
            # Inflate obstacle by player's half-dimensions + padding
            inflated_x = obj.x - player_half_width - OBSTACLE_INFLATION_PADDING
            inflated_y = obj.y - player_half_height - OBSTACLE_INFLATION_PADDING
            inflated_width = obj.width + player_width + 2 * OBSTACLE_INFLATION_PADDING
            inflated_height = (
                obj.height + player_height + 2 * OBSTACLE_INFLATION_PADDING
            )

            # Calculate grid coordinates for the inflated obstacle area
            start_row, start_col = world_to_grid(inflated_x, inflated_y)
            end_row, end_col = world_to_grid(
                inflated_x + inflated_width - 1, inflated_y + inflated_height - 1
            )

            # Clamp grid coordinates to ensure they are within grid bounds
            start_row = max(0, start_row)
            start_col = max(0, start_col)
            end_row = min(grid_rows - 1, end_row)
            end_col = min(grid_cols - 1, end_col)

            for r in range(start_row, end_row + 1):
                for c in range(start_col, end_col + 1):
                    grid[r][c] = 1  # Mark as obstacle

    return grid


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
    player_width: float,  # Added player dimensions for grid creation
    player_height: float,  # Added player dimensions for grid creation
) -> Dict[str, List[GameObject]]:
    """
    Generates map obstacles and safely places portals based on configurations.
    Portals are placed to avoid collision with all objects.
    """
    obstacles = generate_objects(num_obstacles, map_width, map_height)
    portals = []

    for i, config in enumerate(portal_configs):
        initial_center_x = random.uniform(0, map_width)
        initial_center_y = random.uniform(0, map_height)

        portal_x, portal_y = find_valid_position(
            initial_center_x,
            initial_center_y,
            max(map_width, map_height),
            TILE_SIZE,
            TILE_SIZE,
            obstacles
            + portals,  # Check against existing obstacles AND already placed portals
            map_width,
            map_height,
            avoid_passable_collision=True,  # Portals must not collide with any object (even bushes)
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
    {"dest_map": 2, "dest_portal_index": 0, "spawn_radius": 150.0},
    {"dest_map": 1, "dest_portal_index": 0, "spawn_radius": 100.0},
]

portal_configs_map2 = [
    {"dest_map": 1, "dest_portal_index": 0, "spawn_radius": 150.0},
    {"dest_map": 2, "dest_portal_index": 0, "spawn_radius": 100.0},
]

# Player object needs to be defined before map creation to pass its dimensions
# Initial player dimensions (can be changed later if player object is mutable)
temp_player_width = TILE_SIZE
temp_player_height = TILE_SIZE

# Generate map data using the new function, passing player dimensions for obstacle inflation
maps: Dict[int, Dict[str, List[GameObject]]] = {
    1: create_map_data(
        1,
        200,
        portal_configs_map1,
        MAP_WIDTH,
        MAP_HEIGHT,
        temp_player_width,
        temp_player_height,
    ),
    2: create_map_data(
        2,
        200,
        portal_configs_map2,
        MAP_WIDTH,
        MAP_HEIGHT,
        temp_player_width,
        temp_player_height,
    ),
}

# Create navigation grids for each map, passing player dimensions for obstacle inflation
navigation_grids: Dict[int, List[List[int]]] = {
    map_id: create_navigation_grid(
        maps[map_id]["obstacles"],
        MAP_WIDTH,
        MAP_HEIGHT,
        temp_player_width,
        temp_player_height,
    )
    for map_id in maps
}

current_map_id = 1
# Player initial position also needs to be safe and on a walkable A* grid tile
player_initial_x, player_initial_y = find_valid_position(
    MAP_WIDTH / 2,
    MAP_HEIGHT / 2,  # Start search from map center
    min(MAP_WIDTH, MAP_HEIGHT) / 4,  # Search radius for player initial spawn
    temp_player_width,
    temp_player_height,
    maps[current_map_id]["obstacles"]
    + maps[current_map_id]["portals"],  # Check against all objects
    MAP_WIDTH,
    MAP_HEIGHT,
    avoid_passable_collision=False,  # Player can spawn on passable objects (bushes)
    navigation_grid=navigation_grids[current_map_id],  # Ensure it's walkable in A* grid
)
player = Player(
    x=player_initial_x,
    y=player_initial_y,
    width=temp_player_width,
    height=temp_player_height,
)
camera = GameCamera(player, MAP_WIDTH, MAP_HEIGHT)

collided_portal_info: str = "None"
portal_hold_time_display: float = 0.0

active_click_effects: List[ClickEffect] = []  # List to manage click effects


# --- Main Game Loop ---
def main():
    global current_map_id, collided_portal_info, portal_hold_time_display, DEBUG_DRAW_GRID
    pr.init_window(SCREEN_WIDTH, SCREEN_HEIGHT, b"RPG Map Portal System with AOI")
    pr.set_target_fps(60)

    while not pr.window_should_close():
        delta_time = pr.get_frame_time()
        camera.update(delta_time)

        # Toggle debug grid drawing with 'G' key
        if pr.is_key_pressed(pr.KEY_G):
            DEBUG_DRAW_GRID = not DEBUG_DRAW_GRID

        # --- Input Handling (Mouse/Touch) ---
        if pr.is_mouse_button_pressed(pr.MOUSE_LEFT_BUTTON):
            mouse_world_pos = pr.get_screen_to_world_2d(
                pr.get_mouse_position(), camera.cam
            )

            # Add click effect
            active_click_effects.append(
                ClickEffect(x=mouse_world_pos.x, y=mouse_world_pos.y)
            )

            target_grid_pos = world_to_grid(mouse_world_pos.x, mouse_world_pos.y)
            player_grid_pos = world_to_grid(
                player.x + player.width / 2, player.y + player.height / 2
            )  # Use player center for grid pos

            # Ensure target is within map boundaries
            grid = navigation_grids[current_map_id]
            grid_rows = len(grid)
            grid_cols = len(grid[0])

            # --- Handle Player's Current Position ---
            # If player's current position is an obstacle, find the closest walkable tile to start path from
            current_path_start_grid_pos = player_grid_pos
            if not (
                0 <= player_grid_pos[0] < grid_rows
                and 0 <= player_grid_pos[1] < grid_cols
                and grid[player_grid_pos[0]][player_grid_pos[1]] == 0
            ):
                warn(
                    f"Player's current position {player_grid_pos} is an obstacle in the navigation grid. Finding closest walkable start."
                )
                closest_valid_start = find_closest_walkable_grid_pos(
                    player_grid_pos, grid, max_search_radius_tiles=5
                )
                if closest_valid_start:
                    current_path_start_grid_pos = closest_valid_start
                    warn(f"Found closest walkable start at {closest_valid_start}")
                else:
                    warn(
                        f"Could not find a walkable start position near player {player_grid_pos}. Clearing path."
                    )
                    player.path = []
                    player.target_world_pos = None
                    # IMPORTANT: Use 'continue' here to skip the rest of the click handling for this frame
                    continue

            # --- Handle Clicked Target Position ---
            # If clicked target is an obstacle, find the closest walkable tile to path to
            final_target_grid_pos = target_grid_pos
            if not (
                0 <= target_grid_pos[0] < grid_rows
                and 0 <= target_grid_pos[1] < grid_cols
                and grid[target_grid_pos[0]][target_grid_pos[1]] == 0
            ):
                warn(
                    f"Clicked location {target_grid_pos} is an obstacle (after inflation). Finding closest walkable target."
                )
                closest_valid_target = find_closest_walkable_grid_pos(
                    target_grid_pos, grid, max_search_radius_tiles=10
                )
                if closest_valid_target:
                    final_target_grid_pos = closest_valid_target
                    warn(f"Found closest walkable target at {closest_valid_target}")
                else:
                    warn(
                        f"Could not find a walkable target position near click {target_grid_pos}. Clearing path."
                    )
                    player.path = []
                    player.target_world_pos = None
                    # IMPORTANT: Use 'continue' here to skip the rest of the click handling for this frame
                    continue

            # --- Initiate Pathfinding with Validated Start and End Points ---
            path = astar(grid, current_path_start_grid_pos, final_target_grid_pos)
            if path:
                # Apply path smoothing
                smoothed_path = smooth_path(path, grid, player.width, player.height)
                player.path = smoothed_path[
                    1:
                ]  # Skip the first node (current position)
                player.current_path_index = 0
                player.target_world_pos = (
                    mouse_world_pos  # Store the actual clicked world position
                )
            else:
                warn("No path found to (or near) clicked location.")
                player.path = []  # Clear path if no path found
                player.target_world_pos = None

        # Corrected indentation for the else block related to map boundaries
        else:  # This 'else' corresponds to the `if pr.is_mouse_button_pressed`
            pass  # No mouse button pressed, do nothing specific for pathfinding

        # This block was previously incorrectly indented within the 'if mouse button pressed' block
        # and should always execute regardless of mouse input.
        player_center_x, player_center_y = player.get_center()

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
                portal_center_y - portal_center_y
            ) ** 2
            if distance_sq < AOI_RADIUS**2:
                active_portals.append(portal)

        # Pass the current navigation grid to the player update method
        player.update(delta_time, active_obstacles, navigation_grids[current_map_id])

        collided_portal_info = "None"
        portal_hold_time_display = 0.0

        for portal in active_portals:
            if pr.check_collision_recs(player.get_rect(), portal.get_rect()):
                dest_map_id = portal.dest_map
                dest_portal_idx = portal.dest_portal_index

                destination_portal = None
                if dest_portal_idx < len(maps[dest_map_id]["portals"]):
                    destination_portal = maps[dest_map_id]["portals"][dest_portal_idx]
                    collided_portal_info = f"Portal to Map {dest_map_id}, Portal {dest_portal_idx} ({int(destination_portal.x)},{int(destination_portal.y)})"
                else:
                    collided_portal_info = f"Portal link error: Dest portal {dest_portal_idx} not found in Map {dest_map_id}"

                portal_hold_time_display = player.stationary_time

                if player.stationary_time >= TELEPORT_HOLD_TIME and destination_portal:
                    # Find a valid spawn position in the destination map for the player
                    new_map_obstacles = maps[dest_map_id]["obstacles"]

                    # When teleporting, the player's path should be cleared
                    player.path = []
                    player.target_world_pos = None
                    player.current_path_index = 0

                    spawn_x, spawn_y = find_valid_position(
                        destination_portal.get_center()[0],
                        destination_portal.get_center()[1],
                        destination_portal.spawn_radius,
                        player.width,
                        player.height,
                        new_map_obstacles,
                        MAP_WIDTH,
                        MAP_HEIGHT,
                        avoid_passable_collision=False,
                        navigation_grid=navigation_grids[
                            dest_map_id
                        ],  # Ensure new spawn is walkable
                    )
                    current_map_id = dest_map_id
                    player.x, player.y = spawn_x, spawn_y
                    player.stationary_time = 0.0
                    camera.trigger_shake(5.0, 0.2)
                    break

        pr.begin_drawing()
        pr.clear_background(pr.RAYWHITE)
        camera.begin_mode()

        # Draw obstacles first
        for obj in active_obstacles:
            pr.draw_rectangle_rec(obj.get_rect(), obj.color)
            rendered_object_count += 1

        # Draw portals after obstacles
        for portal in active_portals:
            pr.draw_rectangle_rec(portal.get_rect(), portal.color)
            rendered_object_count += 1
            pr.draw_circle_lines(
                int(portal.get_center()[0]),
                int(portal.get_center()[1]),
                int(portal.spawn_radius),
                pr.RED,
            )

        # Draw player last
        pr.draw_rectangle_rec(player.get_rect(), player.color)

        # Draw the current path if available for debugging/visualization
        if player.path:
            for i, grid_pos in enumerate(player.path):
                world_x, world_y = grid_to_world(grid_pos)
                # Draw path segment from current position to next path node
                if i == 0 and player.current_path_index == 0:
                    # Draw line from player's current center to the first path node's center
                    pr.draw_line_ex(
                        pr.Vector2(
                            player.x + player.width / 2, player.y + player.height / 2
                        ),
                        pr.Vector2(world_x, world_y),
                        2,
                        pr.YELLOW,
                    )
                elif i > 0:
                    # Draw line between path nodes
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

        # Draw debug navigation grid
        if DEBUG_DRAW_GRID:
            grid = navigation_grids[current_map_id]
            for r_idx, row in enumerate(grid):
                for c_idx, cell in enumerate(row):
                    if cell == 1:  # Obstacle
                        grid_world_x = c_idx * TILE_SIZE
                        grid_world_y = r_idx * TILE_SIZE
                        pr.draw_rectangle(
                            int(grid_world_x),
                            int(grid_world_y),
                            TILE_SIZE,
                            TILE_SIZE,
                            pr.fade(pr.RED, 0.3),  # Semi-transparent red
                        )

        # Draw active click effects
        for effect in list(
            active_click_effects
        ):  # Iterate over a copy to allow removal
            if effect.update(delta_time):
                active_click_effects.remove(effect)
            else:
                effect.draw(camera.cam.zoom)

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

        # Display current player grid position and target grid position
        player_grid = world_to_grid(player.x, player.y)
        pr.draw_text(f"Player Grid: {player_grid}", 10, 110, 20, pr.DARKGRAY)
        if player.path:
            pr.draw_text(f"Target Grid: {player.path[-1]}", 10, 135, 20, pr.DARKGRAY)
        else:
            pr.draw_text("Target Grid: None", 10, 135, 20, pr.DARKGRAY)

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
