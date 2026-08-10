#include "local_player_view.h"

#include <math.h>
#include <raylib.h>
#include <stdbool.h>

#include "config.h"
#include "local_player.h"
#include "object_layer.h"

/* ── Presentation tuning ─────────────────────────────────────────────────────
 * SAMPLE_MIN_S / SAMPLE_MAX_S — bounds of the interpolation window. The window
 *                comes from the measured interval between two changes of the
 *                predicted position, so it tracks the real cadence: 30 Hz
 *                while the client predicts, 20 Hz while the server drives the
 *                walk.
 * CADENCE_FALL / WINDOW_STRETCH — the cadence estimate rises at once and falls
 *                slowly, and the window is a little longer than the estimate.
 *                A window shorter than the interval completes early and the
 *                sprite stops for the rest of it, which is the judder this
 *                layer removes. A window slightly too long only holds a small
 *                constant lag, which the next window absorbs.
 * SAMPLE_EPSILON_CELLS — advance below this counts as no movement.
 * CATCHUP_SLACK — how much faster than the move speed a reconciliation may
 *                show. Inside the budget, and along the current heading, the
 *                reconciliation is movement the client did not predict, and it
 *                must show. The rest is a misprediction: it goes to the error
 *                offset.
 * ERROR_DECAY_LAMBDA — e-fold rate of the error offset.
 * ERROR_BLEED_FRACTION — share of the travel of the frame that the offset can
 *                take. The rendered motion keeps a forward component.
 * ERROR_MIN_BLEED_CELLS_S — floor of the idle budget, for a move speed of 0.
 * ERROR_MAX_CELLS — offset ceiling. The client shows the excess at once.
 * ERROR_EPSILON_CELLS — sub-pixel residue that one step clears, because the
 *                geometric tail of the decay never gets to 0.
 * SNAP_CELLS   — a jump longer than this is a teleport or a respawn, not a
 *                correction: snap, and drop the offset.
 * WALK_START / WALK_STOP — walk/idle hysteresis, as fractions of the move
 *                speed. The gap keeps small corrections out of the animation.
 * DIRECTION_HOLD_S — facing commits after the motion points to a new octant
 *                for this time.
 * MOTION_DECAY_LAMBDA — decay of the motion rate after the window completes
 *                and no new position arrives. It takes the sprite to idle. */
#define LOCAL_PLAYER_VIEW_SAMPLE_MIN_S ((float)(TICK_DURATION_S * 0.25))
#define LOCAL_PLAYER_VIEW_SAMPLE_MAX_S ((float)(TICK_DURATION_S * 2.5))
#define LOCAL_PLAYER_VIEW_CADENCE_FALL 0.15f
#define LOCAL_PLAYER_VIEW_WINDOW_STRETCH 1.15f
#define LOCAL_PLAYER_VIEW_SAMPLE_EPSILON_CELLS 1e-4f
#define LOCAL_PLAYER_VIEW_CATCHUP_SLACK 0.35f
#define LOCAL_PLAYER_VIEW_ERROR_DECAY_LAMBDA 12.0f
#define LOCAL_PLAYER_VIEW_ERROR_BLEED_FRACTION 0.35f
#define LOCAL_PLAYER_VIEW_ERROR_MIN_BLEED_CELLS_S 0.5f
#define LOCAL_PLAYER_VIEW_ERROR_MAX_CELLS 1.0f
#define LOCAL_PLAYER_VIEW_ERROR_EPSILON_CELLS 0.002f
#define LOCAL_PLAYER_VIEW_SNAP_CELLS 6.0f
#define LOCAL_PLAYER_VIEW_WALK_START_FRACTION 0.25f
#define LOCAL_PLAYER_VIEW_WALK_STOP_FRACTION 0.10f
#define LOCAL_PLAYER_VIEW_DIRECTION_HOLD_S 0.10f
#define LOCAL_PLAYER_VIEW_MOTION_DECAY_LAMBDA 24.0f

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
    Vector2         prev_sim_pos;  /* start of the interpolation window */
    Vector2         curr_sim_pos;  /* newest predicted position */
    float           sample_dt_s;   /* length of the window */
    float           sample_age_s;  /* time inside the window */
    float           cadence_s;     /* interval estimate between two samples */
    Vector2         error_offset;  /* visual-only offset, bleeds toward zero */
    float           bleed_dt_s;
    Vector2         vel;           /* window motion rate, drives the sprite */
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

/* Position of the sim inside the window. It reaches curr_sim_pos at the end
 * of the window and holds there, so the sprite never runs ahead of the sim. */
static Vector2 displayed_sim_pos(void) {
    float alpha = 1.0f;
    if (0.0f < s_view.sample_dt_s) {
        alpha = s_view.sample_age_s / s_view.sample_dt_s;
        if (1.0f < alpha) { alpha = 1.0f; }
    }
    return (Vector2){
        s_view.prev_sim_pos.x + (s_view.curr_sim_pos.x - s_view.prev_sim_pos.x) * alpha,
        s_view.prev_sim_pos.y + (s_view.curr_sim_pos.y - s_view.prev_sim_pos.y) * alpha,
    };
}

static void snap_to(Vector2 sim_pos, Direction sim_direction) {
    s_view.prev_sim_pos = sim_pos;
    s_view.curr_sim_pos = sim_pos;
    s_view.sample_dt_s = (float)TICK_DURATION_S;
    s_view.sample_age_s = s_view.sample_dt_s;
    s_view.cadence_s = (float)TICK_DURATION_S;
    s_view.error_offset = (Vector2){ 0.0f, 0.0f };
    s_view.bleed_dt_s = 0.0f;
    s_view.vel = (Vector2){ 0.0f, 0.0f };
    s_view.mode = MODE_IDLE;
    s_view.direction_hold_s = 0.0f;
    /* A teleport has no organic motion vector — adopt the server's facing. */
    if (DIRECTION_NONE != sim_direction) { s_view.direction = sim_direction; }
    s_view.initialized = true;
}

/* Part of the reconciliation that cannot pass as movement of the player.
 *
 * The client stops predicting once the server acks the input command, so the
 * rest of the walk reaches the client as reconciliation. That part is real
 * movement: it must drive the sprite, not hide in the offset. It passes only
 * as far as it continues the current heading, and only up to the speed of the
 * player. A pull backward, a pull sideways, or a jump is a misprediction: it
 * goes to the offset and bleeds away. */
static Vector2 correction_excess(Vector2 advance, Vector2 correction, float window) {
    float correction_len = sqrtf(correction.x * correction.x + correction.y * correction.y);
    if (LOCAL_PLAYER_VIEW_SAMPLE_EPSILON_CELLS > correction_len) {
        return (Vector2){ 0.0f, 0.0f };
    }

    float predicted_x = advance.x - correction.x;
    float predicted_y = advance.y - correction.y;
    float budget = local_player_move_speed() * window * (1.0f + LOCAL_PLAYER_VIEW_CATCHUP_SLACK) -
                   sqrtf(predicted_x * predicted_x + predicted_y * predicted_y);
    if (0.0f > budget) { budget = 0.0f; }

    /* From rest the correction is the only heading there is. */
    float speed = sqrtf(s_view.vel.x * s_view.vel.x + s_view.vel.y * s_view.vel.y);
    Vector2 heading = { correction.x / correction_len, correction.y / correction_len };
    if (local_player_move_speed() * LOCAL_PLAYER_VIEW_WALK_STOP_FRACTION < speed) {
        heading = (Vector2){ s_view.vel.x / speed, s_view.vel.y / speed };
    }

    float pass = correction.x * heading.x + correction.y * heading.y;
    if (0.0f > pass) { pass = 0.0f; }
    if (budget < pass) { pass = budget; }
    return (Vector2){ correction.x - heading.x * pass, correction.y - heading.y * pass };
}

/* Open a window toward the new predicted position. The window starts where
 * the sprite is now, so the rendered path stays continuous whatever the
 * cadence of the sim. The excess shifts the window and the offset by the same
 * vector, which holds the rendered position and hides the misprediction. */
static void open_sample(Vector2 sim_pos, Vector2 advance, Vector2 correction) {
    /* A gap longer than the bound is an idle period, not a cadence: keep the
     * estimate the walk had. */
    float measured = s_view.sample_age_s;
    if (LOCAL_PLAYER_VIEW_SAMPLE_MAX_S < measured) { measured = s_view.cadence_s; }
    if (LOCAL_PLAYER_VIEW_SAMPLE_MIN_S > measured) { measured = LOCAL_PLAYER_VIEW_SAMPLE_MIN_S; }

    if (s_view.cadence_s < measured) {
        s_view.cadence_s = measured;
    } else {
        s_view.cadence_s += (measured - s_view.cadence_s) * LOCAL_PLAYER_VIEW_CADENCE_FALL;
    }
    float window = s_view.cadence_s * LOCAL_PLAYER_VIEW_WINDOW_STRETCH;

    Vector2 start = displayed_sim_pos();
    Vector2 excess = correction_excess(advance, correction, window);

    s_view.error_offset.x -= excess.x;
    s_view.error_offset.y -= excess.y;
    s_view.prev_sim_pos.x = start.x + excess.x;
    s_view.prev_sim_pos.y = start.y + excess.y;
    s_view.curr_sim_pos = sim_pos;
    s_view.sample_dt_s = window;
    s_view.sample_age_s = 0.0f;
    s_view.vel.x = (sim_pos.x - s_view.prev_sim_pos.x) / window;
    s_view.vel.y = (sim_pos.y - s_view.prev_sim_pos.y) / window;
}

/* The motion rate holds for the whole window. It decays only after the window
 * completes with no new position, which is how the sprite reaches idle. */
static void decay_motion(float dt) {
    if (s_view.sample_age_s < s_view.sample_dt_s) { return; }
    float keep = expf(-LOCAL_PLAYER_VIEW_MOTION_DECAY_LAMBDA * dt);
    s_view.vel.x *= keep;
    s_view.vel.y *= keep;
}

/* Cells the offset can give up this frame. A moving player hides the offset in
 * the travel of the frame, so the render position keeps a forward component
 * whatever the offset direction. An idle player has no travel to hide the
 * offset in, so the offset bleeds on a time budget. */
static float bleed_budget(float travel, float speed, float dt) {
    if (local_player_move_speed() * LOCAL_PLAYER_VIEW_WALK_STOP_FRACTION < speed) {
        return travel * LOCAL_PLAYER_VIEW_ERROR_BLEED_FRACTION;
    }
    float idle_speed = local_player_move_speed() * LOCAL_PLAYER_VIEW_ERROR_BLEED_FRACTION;
    if (LOCAL_PLAYER_VIEW_ERROR_MIN_BLEED_CELLS_S > idle_speed) {
        idle_speed = LOCAL_PLAYER_VIEW_ERROR_MIN_BLEED_CELLS_S;
    }
    return idle_speed * dt;
}

/* Shrink the error offset toward zero. The scale factor stays in [0, 1], so
 * the offset keeps its direction and never overshoots. The decay reads the
 * time since the last step, not the time of the frame, so a frame that bleeds
 * nothing does not slow the decay. */
static void bleed_error_offset(float travel, float speed, float dt) {
    s_view.bleed_dt_s += dt;

    float len = sqrtf(s_view.error_offset.x * s_view.error_offset.x +
                      s_view.error_offset.y * s_view.error_offset.y);
    if (0.0f == len) {
        s_view.bleed_dt_s = 0.0f;
        return;
    }

    float keep = 1.0f;
    if (LOCAL_PLAYER_VIEW_ERROR_MAX_CELLS < len) {
        keep = LOCAL_PLAYER_VIEW_ERROR_MAX_CELLS / len;
        len = LOCAL_PLAYER_VIEW_ERROR_MAX_CELLS;
    }

    float step = len * (1.0f - expf(-LOCAL_PLAYER_VIEW_ERROR_DECAY_LAMBDA * s_view.bleed_dt_s));
    float budget = bleed_budget(travel, speed, dt);
    if (budget < step) { step = budget; }
    if (0.0f < step) {
        s_view.bleed_dt_s = 0.0f;
        /* Clear the sub-pixel residue: the geometric tail never gets to 0. */
        if (LOCAL_PLAYER_VIEW_ERROR_EPSILON_CELLS >= len - step) {
            s_view.error_offset = (Vector2){ 0.0f, 0.0f };
            return;
        }
        keep *= 1.0f - step / len;
    }

    s_view.error_offset.x *= keep;
    s_view.error_offset.y *= keep;
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
    if (0.0f > dt) { dt = 0.0f; }

    Vector2 advance = { sim_pos.x - s_view.curr_sim_pos.x,
                        sim_pos.y - s_view.curr_sim_pos.y };
    bool teleported = !s_view.initialized ||
                      MODE_TELEPORTING == sim_mode ||
                      (LOCAL_PLAYER_VIEW_SNAP_CELLS * LOCAL_PLAYER_VIEW_SNAP_CELLS) <
                          (advance.x * advance.x + advance.y * advance.y);
    if (teleported) {
        snap_to(sim_pos, sim_direction);
        return;
    }

    /* Travel of the render position, offset included: opening a window shifts
     * the interpolation and the offset by the same vector, and that shift is
     * not motion the player sees. */
    Vector2 before = local_player_view_position();
    s_view.sample_age_s += dt;
    if ((LOCAL_PLAYER_VIEW_SAMPLE_EPSILON_CELLS * LOCAL_PLAYER_VIEW_SAMPLE_EPSILON_CELLS) <
        (advance.x * advance.x + advance.y * advance.y)) {
        open_sample(sim_pos, advance, sim_correction);
    } else {
        decay_motion(dt);
    }

    Vector2 after = local_player_view_position();
    float travel = sqrtf((after.x - before.x) * (after.x - before.x) +
                         (after.y - before.y) * (after.y - before.y));
    float speed = sqrtf(s_view.vel.x * s_view.vel.x + s_view.vel.y * s_view.vel.y);

    bleed_error_offset(travel, speed, dt);
    update_mode(speed);
    update_direction(dt);
}

Vector2 local_player_view_position(void) {
    Vector2 shown = displayed_sim_pos();
    return (Vector2){ shown.x + s_view.error_offset.x, shown.y + s_view.error_offset.y };
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
