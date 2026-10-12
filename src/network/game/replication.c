#include "network/game/replication.h"

#include "network/game/client_event.h"
#include "domain/presentation_runtime.h"
#include "domain/self_motion.h"
#include "domain/local_player.h"
#include "domain/vault_ops.h"
#include "game_state.h"
#include "util/log.h"
#include "config.h"

#include <raylib.h>
#include <raymath.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>

/* A replay that ends further than this from the live prediction logs. */
#define REPLAY_EPS 0.01f

/* ── Session ───────────────────────────────────────────────────────────── */

static struct {
    cyberia_frame_t     frame;          /* completed fixed steps in this session */
    cyberia_input_seq_t next_sequence;
} s_sess;

void session_reset(void) {
    s_sess.frame         = 0;
    s_sess.next_sequence = 0;
}

cyberia_input_seq_t session_next_input_sequence(void) { return ++s_sess.next_sequence; }
cyberia_frame_t session_frame(void)                   { return s_sess.frame; }
void session_frame_advance(void)                      { s_sess.frame++; }

/* ── Prediction ────────────────────────────────────────────────────────── */

static struct {
    self_motion_t       prev, curr;     /* the last two fixed steps; the view draws between them */
    self_motion_t       state;          /* the newest server state; the decoder writes it */
    cyberia_frame_t     state_frame;    /* the client step the state stands for; 0 = not known yet */
    cyberia_input_seq_t state_consumed; /* inputConsumedThrough of the state */
    bool                state_new;      /* set by the decoder, cleared by the reconcile */
} s_pred;

void prediction_reset(void) {
    memset(&s_pred, 0, sizeof s_pred);
}

void prediction_on_snapshot(const self_motion_t* state, cyberia_frame_t frame,
                            cyberia_input_seq_t consumed) {
    assert(state);
    s_pred.state          = *state;
    s_pred.state_frame    = frame;
    s_pred.state_consumed = consumed;
    s_pred.state_new      = true;
}

/* Apply the motion of the events that take effect in steps from..to. An
 * event pushed in frame F takes effect in step F + 1. */
static void apply_motion(cyberia_frame_t from, cyberia_frame_t to) {
    int count = 0;
    const client_event_t* list = client_event_list(&count);
    for (int i = 0; i < count; i++) {
        const client_event_t* e = &list[i];
        uint64_t step = (uint64_t)e->frame + 1;
        if (from > step || to < step) continue;
        if (CLIENT_EVENT_PLAYER_ACTION == e->kind) {
            /* The server refuses a tap in stasis or as a ghost. */
            if (local_player_in_stasis() || 0 < game_state_vitals(g_game_state.player.base.id)->respawn_in) continue;
            self_motion_tap(&s_pred.curr, (Vector2){ e->payload.target_x, e->payload.target_y });
        } else if (CLIENT_EVENT_PLAYER_STASIS == e->kind && e->payload.active) {
            self_motion_stop(&s_pred.curr);
        }
    }
}

/* The facts of the unconsumed events, on top of the server copy that the
 * decoder has just written. */
static void apply_facts(void) {
    int count = 0;
    const client_event_t* list = client_event_list(&count);
    for (int i = 0; i < count; i++) {
        const client_event_t* e = &list[i];
        if (CLIENT_EVENT_PLAYER_STASIS == e->kind) {
            local_player_set_stasis(e->payload.active);
        } else if (CLIENT_EVENT_ITEM_OPS == e->kind) {
            vault_ops_apply(e->payload.ops, e->payload.op_count);
        }
    }
}

void prediction_step(cyberia_frame_t s) {
    s_pred.prev = s_pred.curr;
    apply_motion(s, s);
    self_motion_step(&s_pred.curr, local_player_move_speed(), TICK_DURATION_S);
}

void prediction_reconcile(void) {
    if (!s_pred.state_new) return;
    s_pred.state_new = false;
    Vector2 predicted = s_pred.curr.pos;

    s_pred.curr = s_pred.state;                  /* 1. replace */
    client_event_trim(s_pred.state_consumed);    /* 2. drop what the state holds */
    apply_facts();                               /* 3. stasis flag, item ops */
    vault_ops_apply_to_inventory();              /*    the open vault's pending list */

    if (0 == s_pred.state_frame) {               /* 4. no offset yet: apply, do not step */
        apply_motion(0, UINT32_MAX);
    } else {
        apply_motion(0, s_pred.state_frame);     /* 5. in flight, dated before the state */
        for (cyberia_frame_t s = s_pred.state_frame + 1; s <= session_frame(); s++) {
            apply_motion(s, s);                  /* 6. replay to now */
            self_motion_step(&s_pred.curr, local_player_move_speed(), TICK_DURATION_S);
        }
        float error = Vector2Distance(predicted, s_pred.curr.pos);
        if (s_pred.state_frame <= session_frame() && REPLAY_EPS < error) {
            LOG_DEBUG("[REPLAY] frame=%u error=%.3f", session_frame(), error);
        }
    }
    if (MODE_TELEPORTING == g_game_state.player.base.mode) s_pred.prev = s_pred.curr;
}

const self_motion_t* prediction_prev(void) { return &s_pred.prev; }
const self_motion_t* prediction_self(void) { return &s_pred.curr; }

/* ── Interpolation ─────────────────────────────────────────────────────── */

/* Render-time interpolation of remote entities (Gambetta Part III).
 *
 * Each entity carries `pos_prev`, `pos_server`, and `snapshot_time` —
 * the wall-clock instant the snapshot that produced pos_server arrived.
 * Interpolating per-entity instead of per-snapshot tolerates batched or
 * jittered AOI delivery without leaving stale positions in place.
 *
 *   t = (now - entity.snapshot_time) * 1000 / interpolation_ms
 *   clamped to [0, 1]
 *
 * For entities that were teleported (mode == TELEPORTING in the decoder)
 * pos_prev was already snapped to the new server position, so the lerp
 * produces an immediate jump.
 */

static inline float compute_alpha_for(double now, double snapshot_time, int window_ms) {
    if (window_ms <= 0) return 1.0f;
    double t = (now - snapshot_time) * 1000.0 / (double)window_ms;
    if (t < 0.0) t = 0.0;
    if (t > 1.0) t = 1.0;
    return (float)t;
}

static void lerp_entity(EntityID id, double now, int window_ms) {
    RenderState* r = game_state_render(id);
    float t = compute_alpha_for(now, r->snapshot_time, window_ms);
    r->interp_pos = Vector2Lerp(r->pos_prev, game_state_transform(id)->pos, t);
}

void interpolation_compute_view(void) {
    const double now = GetTime();
    const int window_ms = world_interpolation_ms();

    for (int i = 0; i < g_game_state.other_player_count; i++)
        lerp_entity(g_game_state.other_players[i].base.id, now, window_ms);
    for (int i = 0; i < g_game_state.bot_count; i++)
        lerp_entity(g_game_state.bots[i].base.id, now, window_ms);
}
