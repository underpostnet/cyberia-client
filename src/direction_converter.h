#ifndef DIRECTION_CONVERTER_H
#define DIRECTION_CONVERTER_H

/**
 * @brief Converts a direction string (e.g., "down_idle") to its corresponding code (e.g., "08").
 *
 * @param direction The direction string to convert.
 * @return A constant string representing the code, or NULL if the direction is invalid.
 */
const char* get_code_from_direction(const char* direction);

#endif // DIRECTION_CONVERTER_H