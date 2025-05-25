import logging

from raylibpy import (
    Color,
    Vector2,
    BLACK,
    LIGHTGRAY,
    GREEN,
    RED,
)

# --- Logging Configuration ---
logging.basicConfig(
    level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s"
)

from config import (
    CAMERA_SMOOTHNESS,
)

from core.raylib_manager import RaylibManager
from core.game_object import GameObject
from visuals.animation_manager import AnimationManager, Direction, AnimationMode


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
        Draws a game object. If the object has an associated animation,
        it delegates rendering to the AnimationManager. Otherwise, it draws
        a simple rectangle.

        Args:
            game_object (GameObject): The game object to draw.
            current_timestamp (float): The current time (e.g., from time.time()) for animation updates.
        """
        # For simplicity, assuming all GameObjects might have an animation
        # In a real MMO, you'd have a 'display_id' property on GameObject
        # For now, we'll use the object_id as a stand-in for display_id for demo purposes
        # and assume a default animation type if not specified.
        # This part needs to be refined based on actual GameObject structure.

        # Example: if game_object has an 'animation_type' or 'display_id' attribute
        # For this refactoring, we'll assume a 'display_id' for simplicity.
        # If your GameObject doesn't have a display_id, you'd need to add one
        # or have a mapping from object type to display_id.
        display_id = "SKIN_PEOPLE"  # Default for player characters
        if game_object.is_obstacle:
            # Obstacles might not have complex animations, or could have a static "animation"
            # For now, we'll draw them as simple rectangles if no specific animation is defined
            # or if their display_id is not in ANIMATION_DATA.
            # If you want animated obstacles, they need a display_id in ANIMATION_DATA.
            display_id = None  # No animation for generic obstacles for now
        # You could add logic here to determine display_id based on game_object.type, etc.

        if display_id:
            # Determine direction and mode based on object movement or state
            # This is a simplified example; actual logic might involve velocity or server state
            direction = Direction.DOWN  # Default
            animation_mode = AnimationMode.IDLE  # Default

            # If the object is moving (has a path and is not at the end)
            if game_object.path and game_object.path_index < len(game_object.path):
                animation_mode = AnimationMode.WALKING
                # Simple direction calculation (can be improved)
                if game_object.path_index + 1 < len(game_object.path):
                    next_point = game_object.path[game_object.path_index + 1]
                    dx = next_point["X"] - game_object.x
                    dy = next_point["Y"] - game_object.y

                    # Determine direction based on dx, dy
                    if abs(dx) > abs(dy):  # More horizontal movement
                        direction = Direction.RIGHT if dx > 0 else Direction.LEFT
                    else:  # More vertical movement
                        direction = Direction.DOWN if dy > 0 else Direction.UP
                    # Add diagonal logic if needed

            # Get or create the animation instance for this game object
            self.animation_manager.get_or_create_animation(
                obj_id=game_object.obj_id,
                display_id=display_id,
                desired_direction=direction,
                desired_mode=animation_mode,
                target_display_size_pixels=self.object_size,  # Use object_size for rendering size
                timestamp=current_timestamp,
            )
            # Render the animation using the AnimationManager
            self.animation_manager.render_object_animation(
                obj_id=game_object.obj_id,
                screen_x=game_object.x,
                screen_y=game_object.y,
                base_color=game_object.color,  # Use object's color as base/fallback
                timestamp=current_timestamp,
            )
        else:
            # If no animation is defined or found, draw a simple rectangle
            self.raylib_manager.draw_rectangle(
                int(game_object.x),
                int(game_object.y),
                self.object_size,
                self.object_size,
                game_object.color,
            )
            # Draw outline for obstacles
            if game_object.is_obstacle:
                self.raylib_manager.draw_rectangle(
                    int(game_object.x),
                    int(game_object.y),
                    self.object_size,
                    self.object_size,
                    BLACK,
                )  # Fill with black
                self.raylib_manager.draw_rectangle(
                    int(game_object.x) + 2,
                    int(game_object.y) + 2,
                    self.object_size - 4,
                    self.object_size - 4,
                    game_object.color,
                )  # Inner color

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

    # Debug info drawing is now handled directly by RaylibManager in MmoClient
    # or can be added here if it's specific to game rendering context.
    # def draw_debug_info(self, text: str, x: int, y: int, font_size: int, color: Color):
    #     """Draws debug text on the screen."""
    #     self.raylib_manager.draw_text(text, x, y, font_size, color)

    # Camera update logic is now handled by RaylibManager in MmoClient
    # def update_camera(self, target_world_pos: Vector2):
    #     """Smoothly moves the camera towards the target world position."""
    #     self.raylib_manager.update_camera_target(target_world_pos, CAMERA_SMOOTHNESS)
