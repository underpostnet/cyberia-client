import pyray as pr
import random
import time
import math


class FloatingText:
    """Represents a single piece of floating text in the game world."""

    def __init__(
        self,
        text: str,
        pos: pr.Vector2,
        color: pr.Color,
        duration: float,
        font_size: int,
    ):
        self.text = text
        self.pos = pos
        self.color = color
        self.duration = duration
        self.font_size = font_size

        self.start_time = time.time()
        self.alpha = 1.0

        # Give it a random upward velocity
        angle = random.uniform(-0.4, 0.4)  # Radians, slightly to the left or right
        speed = random.uniform(20.0, 30.0)
        self.velocity = pr.Vector2(math.sin(angle) * speed, -math.cos(angle) * speed)


class FloatingTextManager:
    """Manages the lifecycle of all floating text instances."""

    def __init__(self, game_state):
        self.game_state = game_state
        self.texts = []
        self.font_size = 32
        self.shadow_offset = 2
        self.shadow_color = pr.Color(0, 0, 0, 150)
        self.duration = 1.5  # seconds

    def add_life_change_text(
        self, diff: float, entity_pos: pr.Vector2, entity_dims: pr.Vector2
    ):
        """Convenience method to create text for healing or damage."""
        if diff == 0:
            return

        is_heal = diff > 0
        text = f"+{int(diff)}" if is_heal else f"{int(diff)}"
        color = (
            self.game_state.colors.get("heal", pr.GREEN)
            if is_heal
            else self.game_state.colors.get("damage", pr.RED)
        )

        # Start text above the center of the entity
        start_pos_world = pr.Vector2(
            (entity_pos.x + entity_dims.x / 2) * self.game_state.cell_size,
            entity_pos.y * self.game_state.cell_size,
        )

        self.add_text(text, start_pos_world, color)

    def add_text(self, text: str, pos: pr.Vector2, color: pr.Color):
        """Adds a new floating text to be managed."""
        ft = FloatingText(text, pos, color, self.duration, self.font_size)
        self.texts.append(ft)

    def update(self, dt: float):
        """Updates all active floating texts."""
        current_time = time.time()
        # Filter out old texts and update remaining ones
        active_texts = []
        for ft in self.texts:
            elapsed = current_time - ft.start_time
            if elapsed < ft.duration:
                # Update position
                ft.pos.x += ft.velocity.x * dt
                ft.pos.y += ft.velocity.y * dt

                # Fade out in the last portion of its life
                fade_start_time = ft.duration * 0.6
                if elapsed > fade_start_time:
                    ft.alpha = 1.0 - (elapsed - fade_start_time) / (
                        ft.duration - fade_start_time
                    )

                active_texts.append(ft)

        self.texts = active_texts

    def draw(self):
        """Draws all active floating texts. Assumes to be called within a 2D camera mode."""
        for ft in self.texts:
            # Set alpha for fade-out effect
            color = pr.color_alpha(ft.color, ft.alpha)
            shadow_color = pr.color_alpha(self.shadow_color, ft.alpha)

            # Center text horizontally
            text_width = pr.measure_text(ft.text, ft.font_size)
            draw_pos = pr.Vector2(ft.pos.x - text_width / 2, ft.pos.y)

            # Draw shadow
            pr.draw_text(
                ft.text,
                int(draw_pos.x + self.shadow_offset),
                int(draw_pos.y + self.shadow_offset),
                ft.font_size,
                shadow_color,
            )
            # Draw main text
            pr.draw_text(ft.text, int(draw_pos.x), int(draw_pos.y), ft.font_size, color)
