from typing import List


class DirectionConverter:
    # A dictionary for a clean, efficient lookup from a code to a list of directions.
    _code_to_directions = {
        "08": ["down_idle", "none_idle", "default_idle"],
        "18": ["down_walking"],
        "02": ["up_idle"],
        "12": ["up_walking"],
        "04": ["left_idle", "up_left_idle", "down_left_idle"],
        "14": ["left_walking", "up_left_walking", "down_left_walking"],
        "06": ["right_idle", "up_right_idle", "down_right_idle"],
        "16": ["right_walking", "up_right_walking", "down_right_walking"],
    }

    # A more complete reverse dictionary that maps every single direction string
    # back to its code. This handles all the cases you've mentioned.
    _complete_directions_to_code = {
        "down_idle": "08",
        "none_idle": "08",
        "default_idle": "08",
        "down_walking": "18",
        "up_idle": "02",
        "up_walking": "12",
        "left_idle": "04",
        "up_left_idle": "04",
        "down_left_idle": "04",
        "left_walking": "14",
        "up_left_walking": "14",
        "down_left_walking": "14",
        "right_idle": "06",
        "up_right_idle": "06",
        "down_right_idle": "06",
        "right_walking": "16",
        "up_right_walking": "16",
        "down_right_walking": "16",
    }

    def get_directions_from_code(self, code: str) -> List[str]:
        """
        Retrieves the list of directions associated with a given code.

        Returns an empty list if the code is not found.
        """
        return self._code_to_directions.get(code, [])

    def get_code_from_directions(self, directions: List[str]) -> str:
        """
        Retrieves the code for a given list of directions.

        This version iterates through the input list and checks each direction
        against the more complete reverse mapping. It returns the code for the
        first direction found.
        """
        for direction in directions:
            # Use .get() for a safe lookup.
            code = self._complete_directions_to_code.get(direction)
            if code:
                return code

        # Return an empty string if no matching code is found.
        return ""
