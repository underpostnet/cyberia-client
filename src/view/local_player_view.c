#include "local_player_view.h"

#include <assert.h>
#include <math.h>
#include <raylib.h>
#include <raymath.h>

#include "world/object_layer.h"
#include "sim/self_motion.h"

/* Local player's draw footprint bump, a clean +10% so the character stands
 * out without breaking the grid. */
#define LOCAL_PLAYER_VIEW_RENDER_SCALE 1.10f

static struct {
    Vector2         pos;
    Direction       direction;
    ObjectLayerMode mode;
} s_view = { .direction = DIRECTION_DOWN, .mode = MODE_IDLE };

void local_player_view_update(const self_motion_t* prev, const self_motion_t* curr, float alpha) {
    assert(prev && curr);
    s_view.pos  = Vector2Lerp(prev->pos, curr->pos, alpha);
    s_view.mode = 0 < curr->path_count ? MODE_WALKING : MODE_IDLE;
    if (DIRECTION_NONE != curr->direction) s_view.direction = curr->direction;
}

Vector2 local_player_view_position(void) { return s_view.pos; }

Direction local_player_view_direction(void) { return s_view.direction; }

Vector2 local_player_view_facing(void) {
    float angle = (float)s_view.direction * PI * 0.25f;
    return (Vector2){ sinf(angle), -cosf(angle) };
}

ObjectLayerMode local_player_view_mode(void) { return s_view.mode; }

Rectangle local_player_view_scaled_footprint(float pos_x, float pos_y, float width, float height) {
    float scaled_w = width * LOCAL_PLAYER_VIEW_RENDER_SCALE;
    float scaled_h = height * LOCAL_PLAYER_VIEW_RENDER_SCALE;
    return (Rectangle){
        pos_x - (scaled_w - width) * 0.5f,
        pos_y - (scaled_h - height),
        scaled_w,
        scaled_h
    };
}
