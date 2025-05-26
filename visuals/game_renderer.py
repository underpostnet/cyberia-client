import logging

from raylibpy import BLACK, LIGHTGRAY, GREEN, RED, YELLOW, GRAY

# --- Logging Configuration ---
logging.basicConfig(
    level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s"
)


from core.raylib_manager import RaylibManager
from core.game_object import GameObject
from visuals.animation_manager import AnimationManager, Direction, AnimationMode
from data.animations.gfx.click_pointer import GFX_CLICK_POINTER_MATRIX_00
from data.animations.skin.people import SKIN_PEOPLE_MATRIX_08_0


class GameRenderer:
    """
    Handles all rendering logic for the game world, including the grid,
    game objects, and player paths. It uses a centralized RaylibManager
    for drawing primitives and an AnimationManager for rendering animated sprites.
    """

    def __init__(
        self,
        raylib_manager: RaylibManager,
        screen_width: int,
        screen_height: int,
        world_width: int,
        world_height: int,
        object_size: int,
    ):
        """
        Initializes the GameRenderer.

        Args:
            raylib_manager (RaylibManager): The centralized Raylib manager instance.
            screen_width (int): The width of the screen.
            screen_height (int): The height of the screen.
            world_width (int): The width of the game world.
            world_height (int): The height of the game world.
            object_size (int): The size of game objects in pixels.
        """
        self.raylib_manager = raylib_manager
        self.screen_width = screen_width
        self.screen_height = screen_height
        self.world_width = world_width
        self.world_height = world_height
        self.object_size = object_size

        # AnimationManager will be responsible for handling and rendering animated objects
        # It needs the raylib_manager to perform its drawing operations
        self.animation_manager = AnimationManager(raylib_manager)

        logging.info("GameRenderer initialized.")

    def draw_grid(self):
        """Draws the grid lines across the game world."""
        for x in range(0, self.world_width + 1, self.object_size):
            self.raylib_manager.draw_line(x, 0, x, self.world_height, LIGHTGRAY)
        for y in range(0, self.world_height + 1, self.object_size):
            self.raylib_manager.draw_line(0, y, self.world_width, y, LIGHTGRAY)

    def draw_game_object(self, game_object: GameObject, current_timestamp: float):
        """
        Draws a game object, handling different object types and animation layers.

        Args:
            game_object (GameObject): The game object to draw.
            current_timestamp (float): The current time (e.g., from time.time()) for animation updates.
        """

        if game_object.object_type == "player":
            game_object.display_ids = ["SKIN_PEOPLE"]

        # Determine animation mode (IDLE or WALKING)
        animation_mode = AnimationMode.IDLE
        if game_object.path and game_object.path_index < len(game_object.path):
            animation_mode = AnimationMode.WALKING

        if game_object.object_type == "player":
            print(
                "path_index:",
                game_object.path_index,
                "animation_mode:",
                animation_mode,
                "direction:",
                game_object.last_known_direction,
            )

        # Iterate through display_ids for Z-layered rendering
        if len(game_object.display_ids) > 0:
            for display_id in game_object.display_ids:
                # Get or create the animation instance for this game object and layer
                # Use a combined ID for the animation manager cache
                layered_obj_id = f"{game_object.obj_id}_{display_id}"

                dim_num_pixels = 0
                if display_id == "GFX_CLICK_POINTER":
                    dim_num_pixels = len(GFX_CLICK_POINTER_MATRIX_00)
                elif display_id == "SKIN_PEOPLE":
                    dim_num_pixels = len(SKIN_PEOPLE_MATRIX_08_0)

                current_animation = self.animation_manager.get_animation_properties(
                    layered_obj_id
                )

                animation_instance = current_animation["animation_instance"]

                if (
                    not current_animation
                    or animation_instance.direction != game_object.last_known_direction
                    or animation_instance.mode != animation_mode
                ):

                    if game_object.object_type == "player":
                        print(
                            "exec last_known_direction:",
                            game_object.last_known_direction,
                            "exec animation_mode:",
                            animation_mode,
                        )

                    self.animation_manager.get_or_create_animation(
                        obj_id=layered_obj_id,  # Use layered ID for cache
                        display_id=display_id,
                        desired_direction=game_object.last_known_direction,
                        desired_mode=animation_mode,
                        target_display_size_pixels=int(
                            self.object_size / (dim_num_pixels - 1)
                        ),
                        timestamp=current_timestamp,
                    )
                # Render the animation using the AnimationManager
                self.animation_manager.render_object_animation(
                    obj_id=layered_obj_id,  # Use layered ID for rendering
                    screen_x=game_object.x,
                    screen_y=game_object.y,
                    timestamp=current_timestamp,
                )

        else:
            # If no display_ids, draw a simple rectangle
            self.raylib_manager.draw_rectangle(
                int(game_object.x),
                int(game_object.y),
                self.object_size,
                self.object_size,
                BLACK,
            )
            self.raylib_manager.draw_rectangle(
                int(game_object.x) + 2,
                int(game_object.y) + 2,
                self.object_size - 4,
                self.object_size - 4,
                game_object.color,
            )

    def draw_path(self, path: list[dict[str, float]]):
        """Draws the path for a game object."""
        if len(path) < 2:
            return
        for i in range(len(path) - 1):
            start_point = path[i]
            end_point = path[i + 1]
            self.raylib_manager.draw_line(
                int(start_point["X"]),
                int(start_point["Y"]),
                int(end_point["X"]),
                int(end_point["Y"]),
                GREEN,
            )
            self.raylib_manager.draw_circle(
                int(start_point["X"]), int(start_point["Y"]), 5, GREEN
            )  # Mark path points
        self.raylib_manager.draw_circle(
            int(path[-1]["X"]), int(path[-1]["Y"]), 5, RED
        )  # End point
