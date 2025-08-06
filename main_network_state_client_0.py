import pyray as pr
import random
import time

# Game configuration
SCREEN_WIDTH, SCREEN_HEIGHT = 800, 600
TILE_SIZE = 64
PLAYER_SIZE = 20
AOI_RADIUS = 5
CAMERA_SMOOTHING = 0.1
PORTAL_HOLD_TIME = 2.0  # Seconds

pr.init_window(SCREEN_WIDTH, SCREEN_HEIGHT, b"RPG AOI World")
pr.set_target_fps(60)

# Define the player
player = pr.Vector2(5 * TILE_SIZE, 5 * TILE_SIZE)
player_speed = 4

# World definitions (2 static maps)
worlds = {
    0: {"size": (20, 15), "objects": [], "portals": [(10, 7), (19, 14)]},
    1: {"size": (25, 20), "objects": [], "portals": [(12, 10), (0, 0)]},
}

current_map = 0


# Generate static objects for each map
def generate_objects(world):
    width, height = world["size"]
    for _ in range(150):
        x = random.randint(0, width - 1)
        y = random.randint(0, height - 1)
        world["objects"].append(((x, y), random.choice(["rock", "tree"])))


for w in worlds.values():
    generate_objects(w)

portal_timer = 0

camera = pr.Camera2D()
camera.offset = pr.Vector2(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2)
camera.zoom = 1.0


def estimate_memory(objects):
    return len(objects) * 16 / 1024  # Approx KB


while not pr.window_should_close():
    # Player movement
    move = pr.Vector2(0, 0)
    if pr.is_key_down(pr.KEY_RIGHT):
        move.x += 1
    if pr.is_key_down(pr.KEY_LEFT):
        move.x -= 1
    if pr.is_key_down(pr.KEY_UP):
        move.y -= 1
    if pr.is_key_down(pr.KEY_DOWN):
        move.y += 1

    if move.x != 0 or move.y != 0:
        length = (move.x**2 + move.y**2) ** 0.5
        move.x = move.x / length * player_speed
        move.y = move.y / length * player_speed

    map_w, map_h = worlds[current_map]["size"]
    new_x = player.x + move.x
    new_y = player.y + move.y
    padding = 0

    max_x = map_w * TILE_SIZE - PLAYER_SIZE
    max_y = map_h * TILE_SIZE - PLAYER_SIZE
    min_x = 0
    min_y = 0

    if min_x <= new_x <= max_x:
        player.x = new_x
    if min_y <= new_y <= max_y:
        player.y = new_y

    # Smooth camera
    camera.target.x += (player.x - camera.target.x) * CAMERA_SMOOTHING
    camera.target.y += (player.y - camera.target.y) * CAMERA_SMOOTHING

    pr.begin_drawing()
    pr.clear_background(pr.RAYWHITE)
    pr.begin_mode_2d(camera)

    px, py = int(player.x / TILE_SIZE), int(player.y / TILE_SIZE)
    visible = []
    for x in range(px - AOI_RADIUS, px + AOI_RADIUS + 1):
        for y in range(py - AOI_RADIUS, py + AOI_RADIUS + 1):
            if 0 <= x < map_w and 0 <= y < map_h:
                pr.draw_rectangle_lines(
                    x * TILE_SIZE, y * TILE_SIZE, TILE_SIZE, TILE_SIZE, pr.LIGHTGRAY
                )

    for pos, kind in worlds[current_map]["objects"]:
        ox, oy = pos
        if abs(ox - px) <= AOI_RADIUS and abs(oy - py) <= AOI_RADIUS:
            color = pr.BROWN if kind == "rock" else pr.DARKGREEN
            pr.draw_circle(
                ox * TILE_SIZE + TILE_SIZE // 2,
                oy * TILE_SIZE + TILE_SIZE // 2,
                10,
                color,
            )
            visible.append((pos, kind))

    for portal in worlds[current_map]["portals"]:
        pxp, pyp = portal
        color = (
            pr.VIOLET
            if abs(pxp - px) <= AOI_RADIUS and abs(pyp - py) <= AOI_RADIUS
            else pr.GRAY
        )
        pr.draw_rectangle(pxp * TILE_SIZE, pyp * TILE_SIZE, TILE_SIZE, TILE_SIZE, color)

        if int(player.x / TILE_SIZE) == pxp and int(player.y / TILE_SIZE) == pyp:
            if portal_timer == 0:
                portal_timer = time.time()
            elif time.time() - portal_timer > PORTAL_HOLD_TIME:
                current_map = 1 - current_map
                map_w, map_h = worlds[current_map]["size"]
                if portal == (0, 0):
                    player = pr.Vector2(
                        (map_w - 2) * TILE_SIZE, (map_h - 2) * TILE_SIZE
                    )
                else:
                    player = pr.Vector2(2 * TILE_SIZE, 2 * TILE_SIZE)
                portal_timer = 0
                camera.target = player
        else:
            portal_timer = 0

    pr.draw_circle(int(player.x), int(player.y), PLAYER_SIZE, pr.BLUE)
    pr.end_mode_2d()

    # Minimap
    minimap_size = 200
    mm_x = SCREEN_WIDTH - minimap_size - 10
    mm_y = SCREEN_HEIGHT - minimap_size - 10
    pr.draw_rectangle(mm_x, mm_y, minimap_size, minimap_size, pr.LIGHTGRAY)

    mw, mh = worlds[current_map]["size"]
    scale_x = minimap_size / (mw * TILE_SIZE)
    scale_y = minimap_size / (mh * TILE_SIZE)

    for pos, kind in visible:
        x, y = pos
        color = pr.DARKGREEN if kind == "tree" else pr.BROWN
        pr.draw_rectangle(
            mm_x + int(x * TILE_SIZE * scale_x),
            mm_y + int(y * TILE_SIZE * scale_y),
            2,
            2,
            color,
        )

    for pxp, pyp in worlds[current_map]["portals"]:
        pr.draw_rectangle(
            mm_x + int(pxp * TILE_SIZE * scale_x),
            mm_y + int(pyp * TILE_SIZE * scale_y),
            4,
            4,
            pr.PURPLE,
        )

    pr.draw_circle(
        mm_x + int(player.x * scale_x), mm_y + int(player.y * scale_y), 3, pr.BLUE
    )

    pr.draw_text(f"Map: {current_map}", 10, 10, 20, pr.BLACK)
    pr.draw_text(f"Objects: {len(visible)}", 10, 35, 20, pr.BLACK)
    pr.draw_text(f"Mem: {estimate_memory(visible):.1f} KB", 10, 60, 20, pr.BLACK)
    pr.end_drawing()

pr.close_window()
