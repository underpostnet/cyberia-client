import pyray as pr
import random

# Constants
TILE_SIZE = 32
VIEW_MARGIN = 2  # Extra tiles to load beyond screen view
FPS = 60

# Initialize window
SCREEN_WIDTH = 800
SCREEN_HEIGHT = 600
pr.init_window(SCREEN_WIDTH, SCREEN_HEIGHT, b"RPG AOI Demo")
pr.set_target_fps(FPS)

# Calculate dynamic AOI view range based on screen size
TILES_X = SCREEN_WIDTH // TILE_SIZE
TILES_Y = SCREEN_HEIGHT // TILE_SIZE
VIEW_DISTANCE_X = TILES_X // 2 + VIEW_MARGIN
VIEW_DISTANCE_Y = TILES_Y // 2 + VIEW_MARGIN

# Setup camera
camera = pr.Camera2D()
camera.zoom = 1.0
camera.offset = (SCREEN_WIDTH // 2, SCREEN_HEIGHT // 2)

# Player setup
player_pos = [0, 0]

# Simulated world with random objects
game_objects = {}  # {(x, y): object_type}
world_range = 100
for _ in range(1000):
    x = random.randint(-world_range, world_range)
    y = random.randint(-world_range, world_range)
    game_objects[(x, y)] = random.choice(["tree", "rock"])

object_colors = {
    "tree": pr.GREEN,
    "rock": pr.GRAY,
}


def draw_object(x, y, obj_type):
    pr.draw_rectangle(
        x * TILE_SIZE, y * TILE_SIZE, TILE_SIZE, TILE_SIZE, object_colors[obj_type]
    )


def get_aoi_objects(player_x, player_y):
    return {
        (x, y): obj_type
        for (x, y), obj_type in game_objects.items()
        if abs(x - player_x) <= VIEW_DISTANCE_X and abs(y - player_y) <= VIEW_DISTANCE_Y
    }


# Main loop
while not pr.window_should_close():
    # Handle input
    if pr.is_key_down(pr.KEY_RIGHT):
        player_pos[0] += 1
    elif pr.is_key_down(pr.KEY_LEFT):
        player_pos[0] -= 1
    elif pr.is_key_down(pr.KEY_DOWN):
        player_pos[1] += 1
    elif pr.is_key_down(pr.KEY_UP):
        player_pos[1] -= 1

    # Update camera to center on player
    camera.target = (
        player_pos[0] * TILE_SIZE + TILE_SIZE // 2,
        player_pos[1] * TILE_SIZE + TILE_SIZE // 2,
    )

    # Get visible objects
    visible_objects = get_aoi_objects(player_pos[0], player_pos[1])

    # Drawing
    pr.begin_drawing()
    pr.clear_background(pr.RAYWHITE)

    pr.begin_mode_2d(camera)

    for y in range(
        player_pos[1] - VIEW_DISTANCE_Y, player_pos[1] + VIEW_DISTANCE_Y + 1
    ):
        for x in range(
            player_pos[0] - VIEW_DISTANCE_X, player_pos[0] + VIEW_DISTANCE_X + 1
        ):
            pr.draw_rectangle_lines(
                x * TILE_SIZE, y * TILE_SIZE, TILE_SIZE, TILE_SIZE, pr.LIGHTGRAY
            )

    for (x, y), obj_type in visible_objects.items():
        draw_object(x, y, obj_type)

    pr.draw_rectangle(
        player_pos[0] * TILE_SIZE,
        player_pos[1] * TILE_SIZE,
        TILE_SIZE,
        TILE_SIZE,
        pr.RED,
    )

    pr.end_mode_2d()

    pr.draw_text(f"Player: {player_pos[0]}, {player_pos[1]}", 10, 10, 20, pr.DARKGRAY)
    pr.draw_text(f"Loaded objects: {len(visible_objects)}", 10, 35, 20, pr.DARKGRAY)
    pr.end_drawing()

pr.close_window()
