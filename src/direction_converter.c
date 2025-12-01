#include "direction_converter.h"
#include <string.h>
#include <stddef.h>

typedef struct {
    const char* direction;
    const char* code;
} DirectionMapping;

static const DirectionMapping mappings[] = {
    {"down_idle", "08"},
    {"none_idle", "08"},
    {"default_idle", "08"},
    {"down_walking", "18"},
    {"up_idle", "02"},
    {"up_walking", "12"},
    {"left_idle", "04"},
    {"up_left_idle", "04"},
    {"down_left_idle", "04"},
    {"left_walking", "14"},
    {"up_left_walking", "14"},
    {"down_left_walking", "14"},
    {"right_idle", "06"},
    {"up_right_idle", "06"},
    {"down_right_idle", "06"},
    {"right_walking", "16"},
    {"up_right_walking", "16"},
    {"down_right_walking", "16"},
    {NULL, NULL} // Sentinel
};

const char* get_code_from_direction(const char* direction) {
    if (direction == NULL) {
        return NULL;
    }

    for (int i = 0; mappings[i].direction != NULL; i++) {
        if (strcmp(direction, mappings[i].direction) == 0) {
            return mappings[i].code;
        }
    }

    return NULL;
}