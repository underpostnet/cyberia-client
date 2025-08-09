import pyray as pr
import websocket
import threading
import json
import time
import math

# --- Game Constants ---
SCREEN_WIDTH = 800
SCREEN_HEIGHT = 800
GRID_SIZE_W = 100
GRID_SIZE_H = 100
CELL_SIZE = SCREEN_WIDTH / GRID_SIZE_W
FPS = 60
WS_URL = "ws://localhost:8080/ws"

# Colors
COLOR_BACKGROUND = pr.Color(30, 30, 30, 255)
COLOR_OBSTACLE = pr.Color(100, 100, 100, 255)
COLOR_PLAYER = pr.Color(0, 200, 255, 255)
COLOR_OTHER_PLAYER = pr.Color(255, 100, 0, 255)
COLOR_PATH = pr.Color(0, 255, 0, 100)
COLOR_TARGET = pr.Color(255, 255, 0, 255)


# --- Client-side State Management ---
class GameState:
    def __init__(self):
        self.player_id = ""
        self.player_pos = None  # Server position (grid coordinates)
        self.player_screen_pos = None  # Screen position (float for smoothing)
        self.visible_players = {}
        self.visible_obstacles = {}
        self.path = []
        self.target_pos = None
        self.camera = pr.Camera2D()
        self.feedback_message = None
        self.message_timer = 0
        self.mutex = threading.Lock()


game_state = GameState()
ws = None


# --- WebSocket Communication ---
def on_message(ws, message):
    """
    Handles incoming WebSocket messages from the server.
    This function parses the JSON and updates the local game state.
    """
    try:
        data = json.loads(message)
        with game_state.mutex:
            if data["type"] == "aoi_update":
                payload = data["payload"]
                game_state.player_id = payload["PlayerID"]
                new_pos = (payload["PlayerPos"]["X"], payload["PlayerPos"]["Y"])

                # Initialize player_screen_pos on first update
                if game_state.player_pos is None:
                    game_state.player_screen_pos = (
                        new_pos[0] * CELL_SIZE,
                        new_pos[1] * CELL_SIZE,
                    )

                game_state.player_pos = new_pos
                game_state.visible_players = {
                    k: (v["X"], v["Y"]) for k, v in payload["VisiblePlayers"].items()
                }

                # Correctly parse the string keys from the server
                visible_obstacles = {}
                for k, v in payload["VisibleGridObjects"].items():
                    x_str, y_str = k.split(",")
                    visible_obstacles[(int(x_str), int(y_str))] = v
                game_state.visible_obstacles = visible_obstacles

                game_state.path = [(p["X"], p["Y"]) for p in payload["Path"]]
                game_state.target_pos = (
                    (payload["TargetPos"]["X"], payload["TargetPos"]["Y"])
                    if payload["TargetPos"]
                    else None
                )

            elif data["type"] == "path_not_found":
                game_state.feedback_message = data["payload"]
                game_state.message_timer = 3.0  # Display message for 3 seconds

    except json.JSONDecodeError as e:
        print(f"Failed to decode JSON: {e}")


def on_error(ws, error):
    print(f"WebSocket error: {error}")


def on_close(ws, close_status_code, close_msg):
    print(f"WebSocket closed with code {close_status_code}: {close_msg}")


def on_open(ws):
    print("WebSocket connection opened.")


def send_path_request(x, y):
    """
    Sends a path request message to the server.
    """
    if ws and ws.sock and ws.sock.connected:
        message = {"type": "path_request", "payload": {"x": x, "y": y}}
        ws.send(json.dumps(message))


# --- Rendering Logic ---
def draw_grid():
    """
    Draws the background grid lines.
    """
    # The grid is drawn from the top-left of the screen.
    for i in range(GRID_SIZE_W):
        pr.draw_line(
            int(i * CELL_SIZE), 0, int(i * CELL_SIZE), SCREEN_HEIGHT * 2, pr.GRAY
        )
    for i in range(GRID_SIZE_H):
        pr.draw_line(
            0, int(i * CELL_SIZE), SCREEN_WIDTH * 2, int(i * CELL_SIZE), pr.GRAY
        )


def draw_game_objects():
    """
    Draws all objects based on the game state received from the server.
    """
    with game_state.mutex:
        # Draw visible obstacles
        for x, y in game_state.visible_obstacles:
            pr.draw_rectangle(
                int(x * CELL_SIZE),
                int(y * CELL_SIZE),
                int(CELL_SIZE),
                int(CELL_SIZE),
                COLOR_OBSTACLE,
            )

        # Draw the path
        if game_state.path:
            for point in game_state.path:
                pr.draw_rectangle(
                    int(point[0] * CELL_SIZE),
                    int(point[1] * CELL_SIZE),
                    int(CELL_SIZE),
                    int(CELL_SIZE),
                    COLOR_PATH,
                )

        # Draw the target position
        if game_state.target_pos:
            x, y = game_state.target_pos
            pr.draw_rectangle(
                int(x * CELL_SIZE),
                int(y * CELL_SIZE),
                int(CELL_SIZE),
                int(CELL_SIZE),
                COLOR_TARGET,
            )

        # Draw other players
        for player_id, pos in game_state.visible_players.items():
            pr.draw_circle(
                int(pos[0] * CELL_SIZE + CELL_SIZE / 2),
                int(pos[1] * CELL_SIZE + CELL_SIZE / 2),
                int(CELL_SIZE / 2 * 0.8),
                COLOR_OTHER_PLAYER,
            )

        # Draw the main player (on top of everything)
        if game_state.player_screen_pos:
            x, y = game_state.player_screen_pos
            pr.draw_circle(
                int(x + CELL_SIZE / 2),
                int(y + CELL_SIZE / 2),
                int(CELL_SIZE / 2 * 0.9),
                COLOR_PLAYER,
            )


def update_camera():
    """
    Updates the camera position to smoothly follow the player.
    """
    with game_state.mutex:
        if game_state.player_screen_pos:
            player_center = pr.Vector2(
                game_state.player_screen_pos[0] + CELL_SIZE / 2,
                game_state.player_screen_pos[1] + CELL_SIZE / 2,
            )

            # Set the camera target to the player's screen position
            game_state.camera.target = player_center
            game_state.camera.offset = pr.Vector2(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2)
            game_state.camera.rotation = 0.0
            game_state.camera.zoom = 1.0


def update_player_screen_pos():
    """
    Smoothly interpolates the player's screen position towards the latest server position.
    """
    with game_state.mutex:
        if game_state.player_pos and game_state.player_screen_pos:
            target_x = game_state.player_pos[0] * CELL_SIZE
            target_y = game_state.player_pos[1] * CELL_SIZE

            current_x, current_y = game_state.player_screen_pos

            # Simple linear interpolation for smooth movement
            lerp_factor = 0.1  # Adjust for desired smoothness
            new_x = current_x + (target_x - current_x) * lerp_factor
            new_y = current_y + (target_y - current_y) * lerp_factor

            game_state.player_screen_pos = (new_x, new_y)


# --- Main Game Loop ---
def run_game():
    """
    Initializes pyray and runs the main game loop.
    """
    global ws

    pr.init_window(SCREEN_WIDTH, SCREEN_HEIGHT, "MMO Prototype Client")
    pr.set_target_fps(FPS)

    # Start the WebSocket client in a separate thread
    ws = websocket.WebSocketApp(
        WS_URL,
        on_open=on_open,
        on_message=on_message,
        on_error=on_error,
        on_close=on_close,
    )
    ws_thread = threading.Thread(target=ws.run_forever, daemon=True)
    ws_thread.start()

    while not pr.window_should_close():
        # --- Update Game State ---
        update_player_screen_pos()
        update_camera()

        # Update message timer
        with game_state.mutex:
            if game_state.feedback_message:
                game_state.message_timer -= pr.get_frame_time()
                if game_state.message_timer <= 0:
                    game_state.feedback_message = None

        # --- Handle User Input ---
        if pr.is_mouse_button_pressed(pr.MOUSE_LEFT_BUTTON):
            with game_state.mutex:
                # Convert mouse coordinates to world coordinates using the camera
                mouse_screen_pos = pr.get_mouse_position()
                world_pos = pr.get_screen_to_world_2d(
                    mouse_screen_pos, game_state.camera
                )
                grid_x = int(world_pos.x // CELL_SIZE)
                grid_y = int(world_pos.y // CELL_SIZE)
                print(
                    f"Mouse clicked at world ({world_pos.x}, {world_pos.y}), grid ({grid_x}, {grid_y})"
                )
                send_path_request(grid_x, grid_y)

        # --- Draw Frame ---
        pr.begin_drawing()
        pr.clear_background(COLOR_BACKGROUND)

        pr.begin_mode_2d(game_state.camera)
        draw_grid()
        draw_game_objects()
        pr.end_mode_2d()

        # Draw UI on top of the world
        with game_state.mutex:
            if game_state.player_id:
                pr.draw_text(
                    f"Player ID: {game_state.player_id}", 10, 10, 14, pr.RAYWHITE
                )
            if game_state.feedback_message:
                pr.draw_text(game_state.feedback_message, 10, 40, 20, pr.RAYWHITE)

        pr.end_drawing()

    pr.close_window()


if __name__ == "__main__":
    # Give the server a moment to start up
    print("Waiting for server to start...")
    time.sleep(2)
    run_game()
