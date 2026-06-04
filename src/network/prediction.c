#include "prediction.h"

#include "network/session.h"
#include "game_state.h"
#include "domain/local_player.h"
#include "input/input_command.h"
#include "util/log.h"
#include "config.h"

#include <raylib.h>
#include <assert.h>
#include <math.h>

/* Client-side prediction & reconciliation.
 *
 * Determinism with the Go server is preserved by using double-precision
 * arithmetic everywhere (the Go integrator in cyberia-server/game/server.go
 * runs on float64) and by reading the authoritative move speed pushed in
 * every AOI self-player block via local_player_set_move_speed().
 */

#define PREDICTION_RING_CAP 128

/* Producer→consumer hand-off queue: replication enqueues commands as they are
 * built each render frame; prediction_step drains them. Sized for high-latency
 * play (>1 s RTT at the tick rate leaves many commands in flight). */
#define COMMAND_QUEUE_CAP 256

typedef struct {
    input_command_t items[PREDICTION_RING_CAP];
    int head;
    int count;
} input_ring_t;

typedef struct {
    input_command_t items[COMMAND_QUEUE_CAP];
    int head;
    int count;
} command_queue_t;

static command_queue_t s_cmd_q = {0};

void prediction_enqueue_input(const input_command_t* cmd) {
    assert(cmd);
    if (s_cmd_q.count == COMMAND_QUEUE_CAP) {
        LOG_WARN("input command queue full, dropping oldest");
        s_cmd_q.head = (s_cmd_q.head + 1) % COMMAND_QUEUE_CAP;
        s_cmd_q.count--;
    }
    int idx = (s_cmd_q.head + s_cmd_q.count) % COMMAND_QUEUE_CAP;
    s_cmd_q.items[idx] = *cmd;
    s_cmd_q.count++;
}

static bool command_queue_pop(input_command_t* out) {
    if (0 == s_cmd_q.count) { return false; }
    *out = s_cmd_q.items[s_cmd_q.head];
    s_cmd_q.head = (s_cmd_q.head + 1) % COMMAND_QUEUE_CAP;
    s_cmd_q.count--;
    return true;
}

static void command_queue_clear(void) {
    s_cmd_q.head  = 0;
    s_cmd_q.count = 0;
}

static struct {
    Vector2       predicted_pos;
    Vector2       authoritative_pos;
    Vector2       display_pos;
    input_ring_t  unacked;
    bool          initialised;
} g_pred = {0};

/* Exponential-smoothing constant. Same value as before; tuned for 60 FPS
 * to converge a sim-tick step within ~80 ms. */
#define PREDICTION_DISPLAY_LAMBDA 18.0

static void ring_clear(input_ring_t* r) {
    r->head = 0;
    r->count = 0;
}

static void ring_push(input_ring_t* r, const input_command_t* cmd) {
    if (r->count == PREDICTION_RING_CAP) {
        LOG_WARN("prediction replay buffer overflow at %u — dropping oldest",
                 (unsigned)PREDICTION_RING_CAP);
        r->head = (r->head + 1) % PREDICTION_RING_CAP;
        r->count--;
    }
    int idx = (r->head + r->count) % PREDICTION_RING_CAP;
    r->items[idx] = *cmd;
    r->count++;
}

static void ring_drop_acked(input_ring_t* r, cyberia_input_seq_t ack) {
    while (r->count > 0) {
        if (r->items[r->head].sequence > ack) break;
        r->head = (r->head + 1) % PREDICTION_RING_CAP;
        r->count--;
    }
}

/* Step one tick toward the command target. Mirrors server's phaseMovement:
 *
 *   step = move_speed * tickDuration            (cells per tick)
 *   if dist < step: snap; else: walk along direction
 *
 * Both client and server use double-precision sqrt so identical inputs
 * produce byte-identical positions. */
static Vector2 sim_step_one(Vector2 pos, const input_command_t* cmd, double dt) {
    if (cmd->kind != INPUT_KIND_PLAYER_ACTION) return pos;
    double dx = (double)cmd->target_x - (double)pos.x;
    double dy = (double)cmd->target_y - (double)pos.y;
    double dist = sqrt(dx * dx + dy * dy);
    if (dist < 1e-4) return pos;
    double speed = (double)local_player_move_speed();
    double step  = speed * dt;
    if (step > dist) step = dist;
    pos.x = (float)((double)pos.x + (dx / dist) * step);
    pos.y = (float)((double)pos.y + (dy / dist) * step);
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
    g_pred.display_pos       = authoritative_pos;
    ring_clear(&g_pred.unacked);
    command_queue_clear();
}

bool prediction_apply(const input_command_t* cmd) {
    if (!cmd) return false;
    ring_push(&g_pred.unacked, cmd);
    g_pred.predicted_pos = sim_step_one(g_pred.predicted_pos, cmd, TICK_DURATION_S);
    return true;
}

void prediction_step(double tick_dt) {
    /* Drain newly produced input commands. This is the single point where the
     * prediction module consumes input; the replication layer is the producer
     * via prediction_enqueue_input(). */
    input_command_t cmd;
    while (command_queue_pop(&cmd)) {
        ring_push(&g_pred.unacked, &cmd);
        g_pred.predicted_pos = sim_step_one(g_pred.predicted_pos, &cmd, TICK_DURATION_S);
    }

    if (g_pred.unacked.count == 0) return;
    int latest = (g_pred.unacked.head + g_pred.unacked.count - 1) % PREDICTION_RING_CAP;
    g_pred.predicted_pos = sim_step_one(g_pred.predicted_pos, &g_pred.unacked.items[latest], tick_dt);
}

void prediction_reconcile(void) {
    g_pred.authoritative_pos = g_game_state.player.base.pos_server;
    ring_drop_acked(&g_pred.unacked, session_last_acked_input_sequence());

    Vector2 rebased = g_pred.authoritative_pos;
    for (int i = 0; i < g_pred.unacked.count; i++) {
        int idx = (g_pred.unacked.head + i) % PREDICTION_RING_CAP;
        rebased = sim_step_one(rebased, &g_pred.unacked.items[idx], TICK_DURATION_S);
    }
    g_pred.predicted_pos = rebased;
}

void prediction_display_step(double frame_dt) {
    if (frame_dt <= 0.0) return;
    double blend = 1.0 - exp(-PREDICTION_DISPLAY_LAMBDA * frame_dt);
    if (blend < 0.0) blend = 0.0;
    if (blend > 1.0) blend = 1.0;
    g_pred.display_pos.x = (float)((double)g_pred.display_pos.x +
                                   ((double)g_pred.predicted_pos.x - (double)g_pred.display_pos.x) * blend);
    g_pred.display_pos.y = (float)((double)g_pred.display_pos.y +
                                   ((double)g_pred.predicted_pos.y - (double)g_pred.display_pos.y) * blend);
    if (fabsf(g_pred.predicted_pos.x - g_pred.display_pos.x) < 0.001f)
        g_pred.display_pos.x = g_pred.predicted_pos.x;
    if (fabsf(g_pred.predicted_pos.y - g_pred.display_pos.y) < 0.001f)
        g_pred.display_pos.y = g_pred.predicted_pos.y;
}

Vector2 prediction_self_position(void) { return g_pred.display_pos; }
