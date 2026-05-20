#include "prediction.h"
#include "../network/session.h"
#include "../game_state.h"

#include <math.h>
#include <string.h>

/* The replay buffer is a fixed-size ring of unacked InputCommand entries.
 * Sizing: at 30 Hz tick + 100 ms RTT we expect ≤ 3 unacked taps at any
 * moment under normal taps; a power of two of 64 leaves head-room for
 * burst-tap scenarios and slow networks. */
#define PREDICTION_RING_CAP 64

typedef struct {
    input_command_t items[PREDICTION_RING_CAP];
    int head;
    int count;
} input_ring_t;

static struct {
    Vector2       predicted_pos;     /* what we render for the local player */
    Vector2       authoritative_pos; /* latest server snapshot self position */
    input_ring_t  unacked;
    bool          initialised;
} g_pred = {0};

static void ring_clear(input_ring_t* r) {
    r->head = 0;
    r->count = 0;
}

static void ring_push(input_ring_t* r, const input_command_t* cmd) {
    if (r->count == PREDICTION_RING_CAP) {
        /* Overflow: drop oldest by advancing head. */
        r->head = (r->head + 1) % PREDICTION_RING_CAP;
        r->count--;
    }
    int idx = (r->head + r->count) % PREDICTION_RING_CAP;
    r->items[idx] = *cmd;
    r->count++;
}

/* Drop every entry whose sequence ≤ ack. The buffer is monotonic by
 * sequence so we can simply pop from the head until the condition is met. */
static void ring_drop_acked(input_ring_t* r, cyberia_input_seq_t ack) {
    while (r->count > 0) {
        if (r->items[r->head].sequence > ack) break;
        r->head = (r->head + 1) % PREDICTION_RING_CAP;
        r->count--;
    }
}

/* Apply a single command to a working position. The integrator used here
 * mirrors the server's phaseMovement: walk one tick toward the tap target
 * at the player's estimated speed. Determinism note: this *should* match
 * the server byte-for-byte; a follow-up commit will share the integrator
 * across both languages. */
static Vector2 sim_step_one(Vector2 pos, const input_command_t* cmd, double dt) {
    if (cmd->kind != INPUT_KIND_PLAYER_ACTION) return pos;
    float dx = cmd->target_x - pos.x;
    float dy = cmd->target_y - pos.y;
    float dist = sqrtf(dx * dx + dy * dy);
    if (dist < 0.0001f) return pos;
    float speed = g_game_state.player.estimated_speed > 0.1f
                    ? g_game_state.player.estimated_speed : 3.0f;
    float step = (float)(speed * dt);
    if (step > dist) step = dist;
    pos.x += (dx / dist) * step;
    pos.y += (dy / dist) * step;
    return pos;
}

void prediction_init(void) {
    if (g_pred.initialised) return;
    ring_clear(&g_pred.unacked);
    g_pred.initialised = true;
}

void prediction_reset(Vector2 authoritative_pos) {
    g_pred.authoritative_pos = authoritative_pos;
    g_pred.predicted_pos     = authoritative_pos;
    ring_clear(&g_pred.unacked);
}

bool prediction_apply(const input_command_t* cmd) {
    if (!cmd) return false;
    ring_push(&g_pred.unacked, cmd);
    /* Optimistic mutation: walk one tick toward target so the player sees
     * movement immediately. Subsequent prediction_step calls keep walking. */
    g_pred.predicted_pos = sim_step_one(g_pred.predicted_pos, cmd, TICK_DURATION_S);
    return true;
}

void prediction_step(double tick_dt) {
    if (g_pred.unacked.count == 0) return;
    /* Apply the latest target (most recent tap wins, just like the server). */
    int latest = (g_pred.unacked.head + g_pred.unacked.count - 1) % PREDICTION_RING_CAP;
    g_pred.predicted_pos = sim_step_one(g_pred.predicted_pos, &g_pred.unacked.items[latest], tick_dt);
}

void prediction_reconcile(void) {
    /* Pull the latest authoritative self-position from game_state. The
     * decoder writes player.base.pos_server in the self-player block. */
    g_pred.authoritative_pos = g_game_state.player.base.pos_server;
    /* Drop everything ≤ ack and rewind+replay. */
    ring_drop_acked(&g_pred.unacked, session_last_acked_input_sequence());

    Vector2 rebased = g_pred.authoritative_pos;
    for (int i = 0; i < g_pred.unacked.count; i++) {
        int idx = (g_pred.unacked.head + i) % PREDICTION_RING_CAP;
        rebased = sim_step_one(rebased, &g_pred.unacked.items[idx], TICK_DURATION_S);
    }
    g_pred.predicted_pos = rebased;
}

Vector2 prediction_self_position(void) {
    return g_pred.predicted_pos;
}
