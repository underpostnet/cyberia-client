#include "local_player_view.h"

#include <math.h>
#include <raylib.h>
#include <stdbool.h>

#include "config.h"
#include "local_player.h"
#include "object_layer.h"

/* ── Presentation tuning ─────────────────────────────────────────────────────
 * The sprite follows the predicted position at the speed of the player, not at
 * the rate the positions arrive. Arrival is a 30 Hz sim tick while the client
 * predicts, and a jittered 20 Hz snapshot once the server drives the walk.
 * Deriving the rendered speed from the arrival interval puts that jitter on
 * the screen, so the follower keeps a small lag instead and trims it slowly.
 *
 * FOLLOW_BUFFER_FACTOR — lag the follower holds, in cadence intervals. It has
 *                to outlast the gap between two arrivals: a follower that
 *                catches the target stops until the next one lands.
 * FOLLOW_TRIM_S — time constant that returns the lag to the buffer. It is the
 *                only path from arrival jitter to rendered speed, so a longer
 *                value is smoother and slower to recover.
 * FOLLOW_LAG_AVG_S — the trim reads a low-passed lag. The target jumps and the
 *                follower is continuous, so the raw lag carries a sawtooth of
 *                the arrival period; the trim must not put it on the screen.
 * FOLLOW_CATCHUP / FOLLOW_EASE — ceilings of the trim above and below the move
 *                speed. Catching up may be brisk. Building the buffer must be
 *                gentle: it happens at the start of a walk, where any slowdown
 *                would read as input lag.
 * FOLLOW_MAX_LAG_CELLS — hard bound of the lag after a long stall.
 * SAMPLE_MIN_S / SAMPLE_MAX_S — bounds of the measured arrival interval.
 * CADENCE_FALL — the cadence estimate rises at once and falls slowly, so the
 *                buffer covers the worst recent gap.
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
 * IDLE_HOLD_S  — the sprite keeps the walk for this long after the follower
 *                reaches the target, so a stalled link does not flicker the
 *                animation. */
#define LOCAL_PLAYER_VIEW_FOLLOW_BUFFER_FACTOR 1.20f
#define LOCAL_PLAYER_VIEW_FOLLOW_TRIM_S 0.25f
#define LOCAL_PLAYER_VIEW_FOLLOW_LAG_AVG_S 0.30f
#define LOCAL_PLAYER_VIEW_FOLLOW_CATCHUP 0.35f
#define LOCAL_PLAYER_VIEW_FOLLOW_EASE 0.10f
#define LOCAL_PLAYER_VIEW_FOLLOW_MAX_LAG_CELLS 1.5f
#define LOCAL_PLAYER_VIEW_SAMPLE_MIN_S ((float)(TICK_DURATION_S * 0.25))
#define LOCAL_PLAYER_VIEW_SAMPLE_MAX_S ((float)(TICK_DURATION_S * 2.5))
#define LOCAL_PLAYER_VIEW_CADENCE_FALL 0.15f
#define LOCAL_PLAYER_VIEW_SAMPLE_EPSILON_CELLS 1e-4f
#define LOCAL_PLAYER_VIEW_CATCHUP_SLACK 0.35f
#define LOCAL_PLAYER_VIEW_ERROR_DECAY_LAMBDA 12.0f
#define LOCAL_PLAYER_VIEW_ERROR_BLEED_FRACTION 0.25f
#define LOCAL_PLAYER_VIEW_ERROR_MIN_BLEED_CELLS_S 0.5f
#define LOCAL_PLAYER_VIEW_ERROR_MAX_CELLS 1.0f
#define LOCAL_PLAYER_VIEW_ERROR_EPSILON_CELLS 0.002f
#define LOCAL_PLAYER_VIEW_SNAP_CELLS 6.0f
#define LOCAL_PLAYER_VIEW_WALK_START_FRACTION 0.25f
#define LOCAL_PLAYER_VIEW_WALK_STOP_FRACTION 0.10f
#define LOCAL_PLAYER_VIEW_DIRECTION_HOLD_S 0.10f
#define LOCAL_PLAYER_VIEW_IDLE_HOLD_S 0.12f

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
    Vector2         sim_pos;        /* newest predicted position: the target */
    Vector2         follow_pos;     /* what the sprite shows, never jumps */
    float           lag_avg;        /* low-passed distance to the target */
    float           sample_age_s;   /* time since the target last changed */
    float           cadence_s;      /* interval estimate between two changes */
    Vector2         error_offset;   /* visual-only offset, bleeds toward zero */
    float           bleed_dt_s;
    Vector2         vel;            /* follower velocity, drives the sprite */
    Direction       direction;
    float           direction_hold_s;
    ObjectLayerMode mode;
    float           idle_hold_s;
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
    s_view.sim_pos = sim_pos;
    s_view.follow_pos = sim_pos;
    s_view.lag_avg = 0.0f;
    s_view.sample_age_s = 0.0f;
    s_view.cadence_s = (float)TICK_DURATION_S;
    s_view.error_offset = (Vector2){ 0.0f, 0.0f };
    s_view.bleed_dt_s = 0.0f;
    s_view.vel = (Vector2){ 0.0f, 0.0f };
    s_view.mode = MODE_IDLE;
    s_view.direction_hold_s = 0.0f;
    s_view.idle_hold_s = 0.0f;
    /* A teleport has no organic motion vector — adopt the server's facing. */
    if (DIRECTION_NONE != sim_direction) { s_view.direction = sim_direction; }
    s_view.initialized = true;
}

/* Measured interval since the last change of the target, and the cadence
 * estimate it feeds. The estimate rises at once and falls slowly, so the
 * buffer of the follower covers the worst recent gap. */
static float update_cadence(void) {
    float measured = s_view.sample_age_s;
    if (LOCAL_PLAYER_VIEW_SAMPLE_MAX_S < measured) { measured = s_view.cadence_s; }
    if (LOCAL_PLAYER_VIEW_SAMPLE_MIN_S > measured) { measured = LOCAL_PLAYER_VIEW_SAMPLE_MIN_S; }

    if (s_view.cadence_s < measured) {
        s_view.cadence_s = measured;
    } else {
        s_view.cadence_s += (measured - s_view.cadence_s) * LOCAL_PLAYER_VIEW_CADENCE_FALL;
    }
    return measured;
}

/* Part of the reconciliation that cannot pass as movement of the player.
 *
 * The client stops predicting once the server acks the input command, so the
 * rest of the walk reaches the client as reconciliation. That part is real
 * movement: it must drive the sprite, not hide in the offset. It passes only
 * as far as it continues the current heading, and only up to the speed of the
 * player over the interval it covers. A pull backward, a pull sideways, or a
 * jump is a misprediction: it goes to the offset and bleeds away. */
static Vector2 correction_excess(Vector2 advance, Vector2 correction, float interval) {
    float correction_len = sqrtf(correction.x * correction.x + correction.y * correction.y);
    if (LOCAL_PLAYER_VIEW_SAMPLE_EPSILON_CELLS > correction_len) {
        return (Vector2){ 0.0f, 0.0f };
    }

    float predicted_x = advance.x - correction.x;
    float predicted_y = advance.y - correction.y;
    float budget = local_player_move_speed() * interval * (1.0f + LOCAL_PLAYER_VIEW_CATCHUP_SLACK) -
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

/* Walk the follower toward the target and report the cells it covered.
 *
 * The speed is the move speed of the player, trimmed only to hold the lag at
 * the buffer. Arrival jitter changes the lag, never the speed directly, so the
 * rendered motion stays even on a jittered link. The step never passes the
 * target, so the sprite cannot run ahead of the sim or oscillate around it. */
static float follow_step(float dt) {
    float dx = s_view.sim_pos.x - s_view.follow_pos.x;
    float dy = s_view.sim_pos.y - s_view.follow_pos.y;
    float dist = sqrtf(dx * dx + dy * dy);
    if (0.0f >= dt) { return 0.0f; }
    if (LOCAL_PLAYER_VIEW_SAMPLE_EPSILON_CELLS > dist) {
        s_view.follow_pos = s_view.sim_pos;
        s_view.lag_avg = 0.0f;
        s_view.vel = (Vector2){ 0.0f, 0.0f };
        return 0.0f;
    }

    /* A stall longer than the bound is not something to walk off. */
    if (LOCAL_PLAYER_VIEW_FOLLOW_MAX_LAG_CELLS < dist) {
        float k = (dist - LOCAL_PLAYER_VIEW_FOLLOW_MAX_LAG_CELLS) / dist;
        s_view.follow_pos.x += dx * k;
        s_view.follow_pos.y += dy * k;
        dx -= dx * k;
        dy -= dy * k;
        dist = LOCAL_PLAYER_VIEW_FOLLOW_MAX_LAG_CELLS;
    }

    s_view.lag_avg += (dist - s_view.lag_avg) *
                      (1.0f - expf(-dt / LOCAL_PLAYER_VIEW_FOLLOW_LAG_AVG_S));

    float move_speed = local_player_move_speed();
    float buffer = move_speed * s_view.cadence_s * LOCAL_PLAYER_VIEW_FOLLOW_BUFFER_FACTOR;
    float trim = (s_view.lag_avg - buffer) / LOCAL_PLAYER_VIEW_FOLLOW_TRIM_S;
    float catchup = move_speed * LOCAL_PLAYER_VIEW_FOLLOW_CATCHUP;
    float ease = move_speed * LOCAL_PLAYER_VIEW_FOLLOW_EASE;
    if (catchup < trim) { trim = catchup; }
    if (-ease > trim) { trim = -ease; }

    float speed = move_speed + trim;
    if (0.0f > speed) { speed = 0.0f; }
    float step = speed * dt;
    if (dist < step) {
        step = dist;
        speed = step / dt;
    }

    float inv = 1.0f / dist;
    s_view.follow_pos.x += dx * inv * step;
    s_view.follow_pos.y += dy * inv * step;
    s_view.vel.x = dx * inv * speed;
    s_view.vel.y = dy * inv * speed;
    return step;
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

/* The follower stops the moment it reaches the target, and a stalled link
 * makes it reach the target between two arrivals. The hold keeps the walk
 * across such a pause, and still reaches idle quickly on a real stop. */
static void update_mode(float speed, float dt) {
    float move_speed = local_player_move_speed();
    if (MODE_WALKING == s_view.mode) {
        if (move_speed * LOCAL_PLAYER_VIEW_WALK_STOP_FRACTION > speed) {
            s_view.idle_hold_s += dt;
            if (LOCAL_PLAYER_VIEW_IDLE_HOLD_S <= s_view.idle_hold_s) {
                s_view.mode = MODE_IDLE;
            }
        } else {
            s_view.idle_hold_s = 0.0f;
        }
    } else if (move_speed * LOCAL_PLAYER_VIEW_WALK_START_FRACTION < speed) {
        s_view.mode = MODE_WALKING;
        s_view.idle_hold_s = 0.0f;
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

    Vector2 advance = { sim_pos.x - s_view.sim_pos.x,
                        sim_pos.y - s_view.sim_pos.y };
    bool teleported = !s_view.initialized ||
                      MODE_TELEPORTING == sim_mode ||
                      (LOCAL_PLAYER_VIEW_SNAP_CELLS * LOCAL_PLAYER_VIEW_SNAP_CELLS) <
                          (advance.x * advance.x + advance.y * advance.y);
    if (teleported) {
        snap_to(sim_pos, sim_direction);
        return;
    }

    s_view.sample_age_s += dt;
    if ((LOCAL_PLAYER_VIEW_SAMPLE_EPSILON_CELLS * LOCAL_PLAYER_VIEW_SAMPLE_EPSILON_CELLS) <
        (advance.x * advance.x + advance.y * advance.y)) {
        float interval = update_cadence();
        /* The offset and the follower take the same shift, so the misprediction
         * leaves the target without moving the rendered position. */
        Vector2 excess = correction_excess(advance, sim_correction, interval);
        s_view.error_offset.x -= excess.x;
        s_view.error_offset.y -= excess.y;
        s_view.follow_pos.x += excess.x;
        s_view.follow_pos.y += excess.y;
        s_view.sim_pos = sim_pos;
        s_view.sample_age_s = 0.0f;
    }

    float travel = follow_step(dt);
    float speed = sqrtf(s_view.vel.x * s_view.vel.x + s_view.vel.y * s_view.vel.y);

    bleed_error_offset(travel, speed, dt);
    update_mode(speed, dt);
    update_direction(dt);
}

Vector2 local_player_view_position(void) {
    return (Vector2){ s_view.follow_pos.x + s_view.error_offset.x,
                      s_view.follow_pos.y + s_view.error_offset.y };
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
