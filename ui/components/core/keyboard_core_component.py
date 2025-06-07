import logging
from raylibpy import (
    get_char_pressed,
    get_key_pressed,
    is_key_down,
    KEY_BACKSPACE,
    KEY_LEFT_SHIFT,
    KEY_RIGHT_SHIFT,
    KEY_C,
    KEY_V,
    KEY_A,
    KEY_HOME,
    KEY_END,
    KEY_LEFT,
    KEY_RIGHT,
    KEY_DELETE,
)

logging.basicConfig(
    level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s"
)


class KeyboardCoreComponent:
    """
    Manages global keyboard input, tracking key presses, characters, and key down states.
    It also handles backspace repeat delay.
    """

    def __init__(
        self, backspace_initial_delay: float = 0.4, backspace_repeat_rate: float = 0.05
    ):
        self.char_pressed: int | None = None
        self.key_pressed: int | None = None
        self.is_key_down_map: dict[int, bool] = (
            {}
        )  # Tracks which keys are currently held down

        self.backspace_initial_delay = backspace_initial_delay
        self.backspace_repeat_rate = backspace_repeat_rate
        self.backspace_timer = 0.0
        self.is_backspace_held = False

    def update(self, dt: float):
        """
        Updates the keyboard state for the current frame.
        Should be called once per frame.
        Args:
            dt: Delta time from the last frame.
        """
        self.char_pressed = get_char_pressed()
        self.key_pressed = get_key_pressed()

        # Update is_key_down_map for currently held keys using direct raylibpy constants
        self.is_key_down_map[KEY_BACKSPACE] = is_key_down(KEY_BACKSPACE)
        self.is_key_down_map[KEY_LEFT_SHIFT] = is_key_down(KEY_LEFT_SHIFT)
        self.is_key_down_map[KEY_RIGHT_SHIFT] = is_key_down(KEY_RIGHT_SHIFT)
        self.is_key_down_map[KEY_C] = is_key_down(KEY_C)
        self.is_key_down_map[KEY_V] = is_key_down(KEY_V)
        self.is_key_down_map[KEY_A] = is_key_down(KEY_A)
        self.is_key_down_map[KEY_HOME] = is_key_down(KEY_HOME)
        self.is_key_down_map[KEY_END] = is_key_down(KEY_END)
        self.is_key_down_map[KEY_LEFT] = is_key_down(KEY_LEFT)
        self.is_key_down_map[KEY_RIGHT] = is_key_down(KEY_RIGHT)
        self.is_key_down_map[KEY_DELETE] = is_key_down(KEY_DELETE)

        # Handle backspace repeat
        if is_key_down(KEY_BACKSPACE):
            if not self.is_backspace_held:
                # First press, register it as key_pressed and start initial delay
                self.backspace_timer = 0.0
                self.is_backspace_held = True
            else:
                self.backspace_timer += dt
                if self.backspace_timer >= self.backspace_initial_delay:
                    # After initial delay, start repeating
                    if (
                        self.backspace_timer - self.backspace_initial_delay
                    ) % self.backspace_repeat_rate < dt:
                        self.key_pressed = KEY_BACKSPACE  # Simulate repeat key press
        else:
            self.is_backspace_held = False
            self.backspace_timer = 0.0

    def get_char_pressed(self) -> int | None:
        """Returns the character pressed in the current frame, or None."""
        return self.char_pressed

    def get_key_pressed(self) -> int | None:
        """Returns the key code pressed in the current frame, or None."""
        return self.key_pressed

    def get_is_key_down_map(self) -> dict[int, bool]:
        """Returns a map of keys currently held down."""
        return self.is_key_down_map
