#include "local_player_view.h"

#include <math.h>
#include <raylib.h>
#include <stdbool.h>

#include "local_player.h"
#include "object_layer.h"

/* ── Presentation tuning ─────────────────────────────────────────────────────
 * SPRING_OMEGA — stiffness (rad/s) of the critically damped spring. Settles a
 *                correction in ~250 ms with continuous velocity; equivalent
 *                second-order response to the two cascaded λ=18 exponential
 *                filters this layer replaces, so felt latency is unchanged.
 * SNAP_CELLS   — a target jump beyond this (world cells) is a teleport or
 *                respawn, not a reconciliation correction: snap, don't slide.
 * WALK_START / WALK_STOP — walk/idle hysteresis as fractions of the
 *                authoritative move speed. The gap between them absorbs the
 *                velocity transients of small reconciliation corrections so
 *                the sprite never toggles animation on a correction.
 * DIRECTION_HOLD_S — facing commits only after the velocity has pointed to a
 *                new octant for this long; correction transients decay faster
 *                than the hold, so they can never flip the sprite.
 * CORRECTION_BLEED_LAMBDA / CORRECTION_MAX_BLEED_FRACTION / CORRECTION_MAX_CELLS
 *              — reconciliation displacements are absorbed into an offset the
 *                instant they land (the spring target never jumps), then bled
 *                away exponentially with the bleed speed capped at this
 *                fraction of the move speed: bleeding a backward correction
 *                can slow the walk slightly but can never reverse it. Offsets
 *                beyond MAX_CELLS pass the excess straight to the spring. */
#define LOCAL_PLAYER_VIEW_SPRING_OMEGA 18.0f
#define LOCAL_PLAYER_VIEW_SNAP_CELLS 6.0f
#define LOCAL_PLAYER_VIEW_WALK_START_FRACTION 0.25f
#define LOCAL_PLAYER_VIEW_WALK_STOP_FRACTION 0.10f
#define LOCAL_PLAYER_VIEW_DIRECTION_HOLD_S 0.10f
#define LOCAL_PLAYER_VIEW_CORRECTION_BLEED_LAMBDA 8.0f
#define LOCAL_PLAYER_VIEW_CORRECTION_MAX_BLEED_FRACTION 0.35f
#define LOCAL_PLAYER_VIEW_CORRECTION_MAX_CELLS 1.0f

/* Local player's draw footprint bump, a clean +10% so the character stands
 * out without breaking the grid. */
#define LOCAL_PLAYER_VIEW_RENDER_SCALE 1.10f

/* Octant index i (0 = facing +X, increasing clockwise since +Y is down) to
 * the compass Direction it represents. */
static const Direction OCTANT_TO_DIRECTION[8] = {
    DIRECTION_RIGHT, DIRECTION_DOWN_RIGHT, DIRECTION_DOWN, DIRECTION_DOWN_LEFT,
    DIRECTION_LEFT, DIRECTION_UP_LEFT, DIRECTION_UP, DIRECTION_UP_RIGHT,
};

typedef struct {
    Vector2         pos;
    Vector2         vel;
    Vector2         correction; /* un-bled visual offset absorbing reconciliation */
    Direction       direction;
    float           direction_hold_s;
    ObjectLayerMode mode;
    bool            initialized;
} LocalPlayerView;

static LocalPlayerView s_view = { .direction = DIRECTION_DOWN, .mode = MODE_IDLE };

/* Quantizes a motion vector to the nearest of the 8 compass directions. */
static Direction quantize_direction(float dx, float dy) {
    float angle = atan2f(dy, dx);
    int octant = (int)lroundf(angle / (PI / 4.0f));
    octant = ((octant % 8) + 8) % 8;
    return OCTANT_TO_DIRECTION[octant];
}

static void snap_to(Vector2 sim_pos, Direction sim_direction) {
    s_view.pos = sim_pos;
    s_view.vel = (Vector2){ 0.0f, 0.0f };
    s_view.correction = (Vector2){ 0.0f, 0.0f };
    s_view.mode = MODE_IDLE;
    s_view.direction_hold_s = 0.0f;
    /* A teleport has no organic motion vector — adopt the server's facing. */
    if (DIRECTION_NONE != sim_direction) { s_view.direction = sim_direction; }
    s_view.initialized = true;
}

/* Exact closed-form step of a critically damped spring toward `target`:
 * position AND velocity stay continuous for any dt, so behaviour is identical
 * at any frame rate, and a retarget (reconciliation correction) bends the
 * trajectory instead of restarting it. */
static void spring_step(Vector2 target, float dt) {
    const float omega = LOCAL_PLAYER_VIEW_SPRING_OMEGA;
    float decay = expf(-omega * dt);
    float ex = s_view.pos.x - target.x;
    float ey = s_view.pos.y - target.y;
    float bx = s_view.vel.x + omega * ex;
    float by = s_view.vel.y + omega * ey;
    s_view.pos.x = target.x + (ex + bx * dt) * decay;
    s_view.pos.y = target.y + (ey + by * dt) * decay;
    s_view.vel.x = (s_view.vel.x - omega * bx * dt) * decay;
    s_view.vel.y = (s_view.vel.y - omega * by * dt) * decay;
}

/* Absorb this frame's reconciliation displacement and bleed the held offset.
 * Absorbing negates the displacement exactly, so the spring target does not
 * move when a correction lands; the offset then decays exponentially with its
 * speed capped below the walk speed, keeping the visible trajectory monotone.
 * Anything beyond the offset cap passes straight to the spring. */
static void absorb_correction(Vector2 sim_correction, float dt) {
    s_view.correction.x -= sim_correction.x;
    s_view.correction.y -= sim_correction.y;

    float len = sqrtf(s_view.correction.x * s_view.correction.x +
                      s_view.correction.y * s_view.correction.y);
    if (LOCAL_PLAYER_VIEW_CORRECTION_MAX_CELLS < len) {
        float k = LOCAL_PLAYER_VIEW_CORRECTION_MAX_CELLS / len;
        s_view.correction.x *= k;
        s_view.correction.y *= k;
        len = LOCAL_PLAYER_VIEW_CORRECTION_MAX_CELLS;
    }
    if (0.0f == len) return;

    float decay = 1.0f - expf(-LOCAL_PLAYER_VIEW_CORRECTION_BLEED_LAMBDA * dt);
    float step_len = len * decay;
    float max_step = local_player_move_speed() *
                     LOCAL_PLAYER_VIEW_CORRECTION_MAX_BLEED_FRACTION * dt;
    if (max_step < step_len) step_len = max_step;

    float k = 1.0f - step_len / len;
    if (k < 0.0f) k = 0.0f;
    s_view.correction.x *= k;
    s_view.correction.y *= k;
}

static void update_mode(float speed) {
    float move_speed = local_player_move_speed();
    if (MODE_WALKING == s_view.mode) {
        if (move_speed * LOCAL_PLAYER_VIEW_WALK_STOP_FRACTION > speed) {
            s_view.mode = MODE_IDLE;
        }
    } else if (move_speed * LOCAL_PLAYER_VIEW_WALK_START_FRACTION < speed) {
        s_view.mode = MODE_WALKING;
    }
}

static void update_direction(float dt) {
    if (MODE_WALKING != s_view.mode) {
        s_view.direction_hold_s = 0.0f;
        return;
    }
    Direction candidate = quantize_direction(s_view.vel.x, s_view.vel.y);
    if (candidate == s_view.direction) {
        s_view.direction_hold_s = 0.0f;
        return;
    }
    s_view.direction_hold_s += dt;
    if (LOCAL_PLAYER_VIEW_DIRECTION_HOLD_S <= s_view.direction_hold_s) {
        s_view.direction = candidate;
        s_view.direction_hold_s = 0.0f;
    }
}

void local_player_view_update(Vector2 sim_pos, Vector2 sim_correction,
                              Direction sim_direction, ObjectLayerMode sim_mode,
                              float dt) {
    float ex = sim_pos.x - s_view.pos.x;
    float ey = sim_pos.y - s_view.pos.y;
    bool teleported = !s_view.initialized ||
                      MODE_TELEPORTING == sim_mode ||
                      (LOCAL_PLAYER_VIEW_SNAP_CELLS * LOCAL_PLAYER_VIEW_SNAP_CELLS) <
                          (ex * ex + ey * ey);
    if (teleported) {
        snap_to(sim_pos, sim_direction);
        return;
    }

    absorb_correction(sim_correction, dt);
    Vector2 target = { sim_pos.x + s_view.correction.x,
                       sim_pos.y + s_view.correction.y };
    spring_step(target, dt);
    update_mode(sqrtf(s_view.vel.x * s_view.vel.x + s_view.vel.y * s_view.vel.y));
    update_direction(dt);
}

Vector2 local_player_view_position(void) {
    return s_view.pos;
}

Direction local_player_view_direction(void) {
    return s_view.direction;
}

ObjectLayerMode local_player_view_mode(void) {
    return s_view.mode;
}

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
