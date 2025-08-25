import logging
from pyray import (
    get_char_pressed,
    get_key_pressed,
    is_key_down,
    KEY_BACKSPACE,
    KEY_LEFT_SHIFT,
    KEY_RIGHT_SHIFT,
    KEY_LEFT_CONTROL,
    KEY_RIGHT_CONTROL,
    KEY_C,
    KEY_V,
    KEY_X,
    KEY_HOME,
    KEY_END,
    KEY_LEFT,
    KEY_RIGHT,
    KEY_DELETE,
    KEY_A,
)
from typing import Union

logging.basicConfig(
    level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s"
)


class KeyboardCoreComponent:
    """
    Manages global keyboard input, tracking key presses, characters, and key down states.
    It also handles key repeat delays for specified keys (like backspace and arrow keys).
    """

    def __init__(
        self, backspace_initial_delay: float = 0.4, backspace_repeat_rate: float = 0.05
    ):
        self.char_pressed: Union[int, None] = None
        self.key_pressed: Union[int, None] = None
        self.is_key_down_map: dict[int, bool] = (
            {}
        )  # Tracks which keys are currently held down

        self.backspace_initial_delay = backspace_initial_delay
        self.backspace_repeat_rate = backspace_repeat_rate
        self.backspace_timer = 0.0
        self.is_backspace_held = False

        # General key repeat properties
        self._key_repeat_initial_delay = 0.4
        self._key_repeat_rate = 0.05
        # Keys that should have repeat behavior (excluding backspace, which is handled separately)
        self._repeatable_keys = [KEY_LEFT, KEY_RIGHT, KEY_HOME, KEY_END, KEY_DELETE]
        # Dictionary to store timers for each repeatable key currently held down
        self._key_timers: dict[int, float] = {}
        # Dictionary to track if a repeatable key is currently being held for repeat
        self._is_key_held: dict[int, bool] = {}

    def update(self, dt: float):
        """
        Updates the keyboard state, capturing new key presses, characters,
        and handling key repeat for designated keys.
        """
        self.char_pressed = get_char_pressed()
        self.key_pressed = get_key_pressed()  # Captures only the initial press

        # Update general is_key_down map for common modifier keys
        self.is_key_down_map[KEY_LEFT_SHIFT] = is_key_down(KEY_LEFT_SHIFT)
        self.is_key_down_map[KEY_RIGHT_SHIFT] = is_key_down(KEY_RIGHT_SHIFT)
        self.is_key_down_map[KEY_LEFT_CONTROL] = is_key_down(KEY_LEFT_CONTROL)
        self.is_key_down_map[KEY_RIGHT_CONTROL] = is_key_down(KEY_RIGHT_CONTROL)
        self.is_key_down_map[KEY_C] = is_key_down(KEY_C)
        self.is_key_down_map[KEY_V] = is_key_down(KEY_V)
        self.is_key_down_map[KEY_X] = is_key_down(KEY_X)
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
                    # This condition checks if a repeat interval has passed
                    if (
                        self.backspace_timer - self.backspace_initial_delay
                    ) % self.backspace_repeat_rate < dt:
                        self.key_pressed = KEY_BACKSPACE  # Simulate repeat key press
        else:
            self.is_backspace_held = False
            self.backspace_timer = 0.0

        # Handle general key repeat for other repeatable keys
        for key in self._repeatable_keys:
            if is_key_down(key):
                if not self._is_key_held.get(key, False):
                    # Initial press of a repeatable key
                    self._key_timers[key] = 0.0
                    self._is_key_held[key] = True
                    # The initial press is already captured by get_key_pressed()
                else:
                    self._key_timers[key] += dt
                    if self._key_timers[key] >= self._key_repeat_initial_delay:
                        # After initial delay, check for repeat interval
                        if (
                            self._key_timers[key] - self._key_repeat_initial_delay
                        ) % self._key_repeat_rate < dt:
                            self.key_pressed = key  # Simulate repeat key press
            else:
                # Key is released, reset its state
                if key in self._is_key_held:
                    self._is_key_held[key] = False
                    self._key_timers[key] = 0.0

    def get_char_pressed(self) -> Union[int, None]:
        """Returns the character pressed in the current frame, or None."""
        return self.char_pressed

    def get_key_pressed(self) -> Union[int, None]:
        """
        Returns the key code pressed in the current frame, or None.
        This includes the initial press and subsequent repeated presses due to holding.
        """
        return self.key_pressed

    def get_is_key_down_map(self) -> dict[int, bool]:
        """Returns a map of key codes to their current down state."""
        return self.is_key_down_map
