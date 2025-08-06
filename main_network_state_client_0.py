import pyray as pr
import random
import os
import psutil

# Constants
TILE_SIZE = 32
VIEW_MARGIN = 2
FPS = 60

# Initialize window
SCREEN_WIDTH = 800
SCREEN_HEIGHT = 600
pr.init_window(SCREEN_WIDTH, SCREEN_HEIGHT, b"RPG AOI Toroidal")
pr.set_target_fps(FPS)

# AOI
TILES_X = SCREEN_WIDTH // TILE_SIZE
TILES_Y = SCREEN_HEIGHT // TILE_SIZE
VIEW_DISTANCE_X = TILES_X // 2 + VIEW_MARGIN
VIEW_DISTANCE_Y = TILES_Y // 2 + VIEW_MARGIN

# World size (toroidal limits)
WORLD_WIDTH = 100
WORLD_HEIGHT = 100

# Camera
camera = pr.Camera2D()
camera.zoom = 1.0
camera.offset = (SCREEN_WIDTH // 2, SCREEN_HEIGHT // 2)
CAMERA_SMOOTHNESS = 0.4

# Player
player_pos = [0, 0]

# Objects
game_objects = {}  # {(x, y): object_type}
for _ in range(1000):
    x = random.randint(0, WORLD_WIDTH - 1)
    y = random.randint(0, WORLD_HEIGHT - 1)
    game_objects[(x, y)] = random.choice(["tree", "rock"])

object_colors = {
    "tree": pr.GREEN,
    "rock": pr.GRAY,
}


def wrap_position(x, y):
    return x % WORLD_WIDTH, y % WORLD_HEIGHT


def draw_object(x, y, obj_type):
    pr.draw_rectangle(
        x * TILE_SIZE, y * TILE_SIZE, TILE_SIZE, TILE_SIZE, object_colors[obj_type]
    )


def get_aoi_objects(px, py):
    visible = {}
    for dy in range(-VIEW_DISTANCE_Y, VIEW_DISTANCE_Y + 1):
        for dx in range(-VIEW_DISTANCE_X, VIEW_DISTANCE_X + 1):
            wx, wy = wrap_position(px + dx, py + dy)
            obj = game_objects.get((wx, wy))
            if obj:
                visible[(wx, wy)] = obj
    return visible


# Main loop
while not pr.window_should_close():
    # Input
    if pr.is_key_down(pr.KEY_RIGHT):
        player_pos[0] = (player_pos[0] + 1) % WORLD_WIDTH
    elif pr.is_key_down(pr.KEY_LEFT):
        player_pos[0] = (player_pos[0] - 1) % WORLD_WIDTH
    elif pr.is_key_down(pr.KEY_DOWN):
        player_pos[1] = (player_pos[1] + 1) % WORLD_HEIGHT
    elif pr.is_key_down(pr.KEY_UP):
        player_pos[1] = (player_pos[1] - 1) % WORLD_HEIGHT

    # Smooth camera
    desired = pr.Vector2(
        player_pos[0] * TILE_SIZE + TILE_SIZE // 2,
        player_pos[1] * TILE_SIZE + TILE_SIZE // 2,
    )
    camera.target = pr.vector2_lerp(camera.target, desired, CAMERA_SMOOTHNESS)

    # Visible objects only
    visible_objects = get_aoi_objects(player_pos[0], player_pos[1])

    # Memory and CPU (approximate usage of visible only)
    object_count = len(visible_objects)
    estimated_kb = object_count * 32  # assume ~32 bytes per object for estimation
    cpu_percent = psutil.Process(os.getpid()).cpu_percent(interval=0)

    # Draw
    pr.begin_drawing()
    pr.clear_background(pr.RAYWHITE)
    pr.begin_mode_2d(camera)

    # Draw grid and objects in view
    for dy in range(-VIEW_DISTANCE_Y, VIEW_DISTANCE_Y + 1):
        for dx in range(-VIEW_DISTANCE_X, VIEW_DISTANCE_X + 1):
            wx, wy = wrap_position(player_pos[0] + dx, player_pos[1] + dy)
            screen_x = wx * TILE_SIZE
            screen_y = wy * TILE_SIZE
            pr.draw_rectangle_lines(
                screen_x, screen_y, TILE_SIZE, TILE_SIZE, pr.LIGHTGRAY
            )
            obj = visible_objects.get((wx, wy))
            if obj:
                draw_object(wx, wy, obj)

    pr.draw_rectangle(
        player_pos[0] * TILE_SIZE,
        player_pos[1] * TILE_SIZE,
        TILE_SIZE,
        TILE_SIZE,
        pr.RED,
    )
    pr.end_mode_2d()

    # UI
    pr.draw_text(f"Player: {player_pos[0]}, {player_pos[1]}", 10, 10, 20, pr.DARKGRAY)
    pr.draw_text(f"Visible objects: {object_count}", 10, 35, 20, pr.DARKGRAY)
    pr.draw_text(f"Estimated Mem: {estimated_kb} KB", 10, 60, 20, pr.DARKGRAY)
    pr.draw_text(f"CPU: {cpu_percent:.1f}%", 10, 85, 20, pr.DARKGRAY)
    pr.end_drawing()

pr.close_window()
