#include "network/replication.h"

#include "game_state.h"
#include "util/serial.h"
#include "input/input_command.h"
#include "input/input.h"
#include "network/game_client.h"
#include "domain/local_player.h"
#include "util/log.h"
#include "config.h"

#include <raylib.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <math.h>

static bool send_event_tap(Vector2 grid, uint32_t client_tick, uint32_t sequence) {
    return network_send(json_pack_player_action(grid.x, grid.y, client_tick, sequence));
}

void replication_prepare_input(input_queue_t in_queue) {
    // in_queue is a deep copy, safe to drain.
    input_event_t evt = { 0 };
    while (input_pop(&in_queue, &evt)) {
        if (INPUT_TAP == evt.type) {
            float cell = g_game_state.cell_size > 0.0f ? g_game_state.cell_size : 12.0f;
            float gx = evt.world_position.x / cell;
            float gy = evt.world_position.y / cell;
            input_command_t cmd = input_command_build_tap(gx, gy);
            prediction_enqueue_input(&cmd);
            send_event_tap((Vector2){gx, gy}, cmd.client_tick, cmd.sequence);
            g_game_state.player.tap_target     = (Vector2){gx, gy};
            g_game_state.player.has_tap_target = true;
        }
    }
}

/* ── Session ───────────────────────────────────────────────────────────── */

/* Singleton session state. Owned by this translation unit; readers go
 * through the accessor functions in replication.h. */
static struct {
    cyberia_tick_t       last_server_tick;
    cyberia_input_seq_t  last_acked_input_sequence;
    cyberia_input_seq_t  last_movement_sequence;
    cyberia_input_seq_t  next_input_sequence;
    double               last_snapshot_wall_time; /* GetTime() when snapshot arrived */
} g_sess = {0};

void session_on_snapshot(uint32_t snapshot_tick, uint32_t last_acked_sequence,
                         uint32_t last_movement_sequence) {
    /* Monotonic by construction — drop out-of-order snapshots. The server
     * never decreases tick; UDP-like reordering could only matter on a
     * WebRTC fork. For WebSocket we still defend against bugs. */
    if (snapshot_tick >= g_sess.last_server_tick) {
        g_sess.last_server_tick        = snapshot_tick;
        g_sess.last_snapshot_wall_time = GetTime();
    }
    if (last_acked_sequence > g_sess.last_acked_input_sequence) {
        g_sess.last_acked_input_sequence = last_acked_sequence;
    }
    if (last_movement_sequence > g_sess.last_movement_sequence) {
        g_sess.last_movement_sequence = last_movement_sequence;
    }
}

cyberia_tick_t session_last_server_tick(void) {
    return g_sess.last_server_tick;
}

cyberia_input_seq_t session_last_acked_input_sequence(void) {
    return g_sess.last_acked_input_sequence;
}

cyberia_input_seq_t session_last_movement_sequence(void) {
    return g_sess.last_movement_sequence;
}

cyberia_tick_t session_server_tick_estimate(void) {
    if (g_sess.last_server_tick == 0) return 0;
    double elapsed = GetTime() - g_sess.last_snapshot_wall_time;
    if (elapsed < 0.0) elapsed = 0.0;
    uint32_t ticks_since = (uint32_t)(elapsed / TICK_DURATION_S);
    return g_sess.last_server_tick + ticks_since;
}

cyberia_tick_t session_render_tick(void) {
    cyberia_tick_t est = session_server_tick_estimate();
    /* Render-tick offset = the runtime interpolation window expressed in
     * ticks. Single source of truth with interpolation_compute_view, which
     * reads the same window in ms. Falls back to the compile-time bootstrap
     * default until the client-hints window is hydrated. */
    int window_ms = g_game_state.interpolation_ms;
    uint32_t offset = (window_ms > 0)
        ? (uint32_t)((window_ms * TICK_RATE_HZ + 500) / 1000)
        : INTERP_TICKS;
    if (0 == offset) { offset = INTERP_TICKS; }
    if (est <= offset) { return 0; }
    return est - offset;
}

cyberia_input_seq_t session_next_input_sequence(void) {
    return ++g_sess.next_input_sequence;
}

/* ── Prediction ────────────────────────────────────────────────────────── */

/* Client-side prediction & reconciliation.
 *
 * Determinism with the Go server is preserved by using double-precision
 * arithmetic everywhere (the Go integrator in cyberia-server/game/server.go
 * runs on float64) and by reading the authoritative move speed pushed in
 * every AOI self-player block via local_player_set_move_speed().
 */

/* Trail of predicted positions, one per sim tick, each stamped with the server
 * tick the client believed was current. Reconciliation reads it to compare the
 * authoritative position against what this client predicted for that same tick.
 * 128 ticks is >2 s at the tick rate — beyond any playable latency. */
#define PREDICTION_HISTORY_CAP 128

/* Producer→consumer hand-off queue: replication enqueues commands as they are
 * built each render frame; prediction_step drains them. Sized for high-latency
 * play (>1 s RTT at the tick rate leaves many commands in flight). */
#define COMMAND_QUEUE_CAP 256

/* How long an unconfirmed command holds off authority.
 *
 * Prediction re-targets on every tap, and the server re-plans on the next tick,
 * so a command is normally confirmed by moveAck within one round trip. Until it
 * is, the snapshot still describes the previous walk and adopting its route
 * would undo the turn the player just asked for. Past this bound the command
 * was superseded rather than delayed — another tap in the same tick outran it —
 * and authority wins, so prediction can never strand itself on a target the
 * server never planned. Sized for a round trip far worse than playable. */
#define MOVE_CONFIRM_TIMEOUT_S 1.0

typedef struct {
    cyberia_tick_t tick;
    Vector2        pos;
} pred_sample_t;

typedef struct {
    pred_sample_t items[PREDICTION_HISTORY_CAP];
    int head;
    int count;
} pred_history_t;

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
    /* Net displacement reconciliation applied to predicted_pos since the last
     * prediction_consume_correction() — read once per render frame by the
     * presentation layer so it can absorb corrections without breaking the
     * visual trajectory. Presentation-only; never read by the simulation. */
    Vector2       correction_accum;
    /* The destination the walk is heading for: the newest movement command
     * sent. A tap is a destination, not a one-tick impulse — the server keeps
     * walking toward it long after it acknowledges the command, so prediction
     * must keep walking too. Dropping it on acknowledgement froze prediction
     * between snapshots and left the walk moving at the snapshot rate. */
    input_command_t active;
    bool            has_active;
    /* The instant after which an `active` still unconfirmed by moveAck yields
     * to authority. */
    double          active_confirm_deadline;
    /* The authoritative route. The server does not walk straight at the tap: it
     * follows an A* polyline cell by cell. Predicting a straight line cuts every
     * corner the route takes, and each snapshot drags the player back onto it —
     * the residual reverse flicker. Following the same waypoints removes the
     * divergence at its source. Empty until the first snapshot of a walk, where
     * the straight line to the tap is still the best guess available. */
    Vector2         path[MAX_PATH_POINTS];
    int             path_count;
    int             path_index;
    pred_history_t  history;
    bool            initialised;
} g_pred = {0};

static void path_clear(void) {
    g_pred.path_count = 0;
    g_pred.path_index = 0;
}

/* Adopt `cmd` as the walk target. The newest destination always wins and always
 * takes effect at once — the server re-plans on the next tick, so there is no
 * cadence for the client to wait for. Sole writer of the walk target from a
 * client command; the authoritative route replaces it once moveAck confirms it.
 *
 * Dropping the authoritative path is the point: it belongs to the destination
 * the player just abandoned, and following it one tick longer is the backward
 * step. Until the new route arrives, the straight line to the tap is the best
 * guess available, and it is the one that starts in the right direction. */
static void retarget_walk(const input_command_t* cmd) {
    g_pred.active                  = *cmd;
    g_pred.has_active              = true;
    g_pred.active_confirm_deadline = GetTime() + MOVE_CONFIRM_TIMEOUT_S;
    path_clear();
}

static void history_clear(pred_history_t* h) {
    h->head = 0;
    h->count = 0;
}

/* One sample per tick. A repeated tick overwrites, so a tick estimate that
 * stalls cannot flood the trail with duplicates of the same instant. */
static void history_push(pred_history_t* h, cyberia_tick_t tick, Vector2 pos) {
    if (0 < h->count) {
        int last = (h->head + h->count - 1) % PREDICTION_HISTORY_CAP;
        if (h->items[last].tick == tick) {
            h->items[last].pos = pos;
            return;
        }
    }
    if (PREDICTION_HISTORY_CAP == h->count) {
        h->head = (h->head + 1) % PREDICTION_HISTORY_CAP;
        h->count--;
    }
    int idx = (h->head + h->count) % PREDICTION_HISTORY_CAP;
    h->items[idx] = (pred_sample_t){ .tick = tick, .pos = pos };
    h->count++;
}

/* Newest sample at or before `tick`, or NULL when the trail does not reach
 * back that far. Samples are pushed in tick order, so this scans forward. */
static const pred_sample_t* history_at(const pred_history_t* h, cyberia_tick_t tick) {
    const pred_sample_t* found = NULL;
    for (int i = 0; i < h->count; i++) {
        int idx = (h->head + i) % PREDICTION_HISTORY_CAP;
        if (h->items[idx].tick > tick) { break; }
        found = &h->items[idx];
    }
    return found;
}

/* Learning the error at one tick revises the whole trail: every later sample
 * was built on the same wrong belief and carries the same offset. */
static void history_shift(pred_history_t* h, Vector2 delta) {
    for (int i = 0; i < h->count; i++) {
        int idx = (h->head + i) % PREDICTION_HISTORY_CAP;
        h->items[idx].pos.x += delta.x;
        h->items[idx].pos.y += delta.y;
    }
}

/* Samples older than the newest authoritative tick can never be compared
 * again. */
static void history_prune(pred_history_t* h, cyberia_tick_t tick) {
    while (0 < h->count && h->items[h->head].tick < tick) {
        h->head = (h->head + 1) % PREDICTION_HISTORY_CAP;
        h->count--;
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

/* One tick along the authoritative route. Mirrors updatePlayerPosition in
 * cyberia-server/game/server.go, including the part that looks wasteful: on
 * reaching a waypoint the server snaps to it and spends the rest of the tick
 * there rather than carrying the remaining budget into the next segment.
 * Prediction copies that, because matching the server matters more here than
 * covering the extra fraction of a cell. */
static Vector2 sim_step_path(Vector2 pos, double dt) {
    Vector2 node = g_pred.path[g_pred.path_index];
    double dx = (double)node.x - (double)pos.x;
    double dy = (double)node.y - (double)pos.y;
    double dist = sqrt(dx * dx + dy * dy);
    double step = (double)local_player_move_speed() * dt;
    if (dist < step) {
        g_pred.path_index++;
        return node;
    }
    pos.x = (float)((double)pos.x + (dx / dist) * step);
    pos.y = (float)((double)pos.y + (dy / dist) * step);
    return pos;
}

void prediction_init(void) {
    if (g_pred.initialised) return;
    history_clear(&g_pred.history);
    g_pred.initialised = true;
}

void prediction_reset(Vector2 authoritative_pos) {
    g_pred.authoritative_pos = authoritative_pos;
    g_pred.predicted_pos     = authoritative_pos;
    g_pred.correction_accum  = (Vector2){ 0.0f, 0.0f };
    g_pred.has_active        = false;
    g_pred.active_confirm_deadline = 0.0;
    path_clear();
    history_clear(&g_pred.history);
    command_queue_clear();
}

bool prediction_apply(const input_command_t* cmd) {
    if (!cmd) return false;
    if (INPUT_KIND_PLAYER_ACTION == cmd->kind) { retarget_walk(cmd); }
    g_pred.predicted_pos = sim_step_one(g_pred.predicted_pos, cmd, TICK_DURATION_S);
    return true;
}

void prediction_step(double tick_dt) {
    /* Drain newly produced input commands. This is the single point where the
     * prediction module consumes input; the replication layer is the producer
     * via prediction_enqueue_input(). The newest destination wins outright, as
     * it does on the server: taps drained in one tick describe one instant, and
     * only the last of them says where the player wants to go. */
    input_command_t cmd;
    while (command_queue_pop(&cmd)) {
        if (INPUT_KIND_PLAYER_ACTION == cmd.kind) { retarget_walk(&cmd); }
    }

    /* Exactly one step per tick, on the same integrator the server runs. The
     * authoritative route wins once it is known; before the first snapshot of a
     * walk the straight line to the tap keeps the start responsive. */
    if (g_pred.path_index < g_pred.path_count) {
        g_pred.predicted_pos = sim_step_path(g_pred.predicted_pos, tick_dt);
    } else if (g_pred.has_active) {
        g_pred.predicted_pos = sim_step_one(g_pred.predicted_pos, &g_pred.active, tick_dt);
    }
    history_push(&g_pred.history, session_server_tick_estimate(), g_pred.predicted_pos);
}

static void adopt_authoritative_target(void);
static Vector2 sim_step_path(Vector2 pos, double dt);

/* Reconciliation compares like with like.
 *
 * A snapshot describes tick T, which is already in the past by one network
 * trip, and the client is deliberately ahead of it. Measuring the authoritative
 * position against the CURRENT predicted position therefore reads that lead as
 * error and drags the player backward on every snapshot. Instead, look up what
 * this client predicted for tick T and difference against that: what remains is
 * misprediction alone. Applying it to the whole trail keeps the lead intact and
 * leaves the walk moving forward. */
void prediction_reconcile(void) {
    g_pred.authoritative_pos = g_game_state.player.base.pos_server;
    cyberia_tick_t snapshot_tick = session_last_server_tick();

    const pred_sample_t* predicted_then = history_at(&g_pred.history, snapshot_tick);
    if (!predicted_then) {
        /* No prediction covers that tick — a fresh session, a stall, or a
         * world rebuild. Authority wins outright. */
        Vector2 before = g_pred.predicted_pos;
        g_pred.predicted_pos = g_pred.authoritative_pos;
        g_pred.correction_accum.x += g_pred.predicted_pos.x - before.x;
        g_pred.correction_accum.y += g_pred.predicted_pos.y - before.y;
        history_clear(&g_pred.history);
        return;
    }

    Vector2 error = { g_pred.authoritative_pos.x - predicted_then->pos.x,
                      g_pred.authoritative_pos.y - predicted_then->pos.y };
    if (0.0f != error.x || 0.0f != error.y) {
        g_pred.predicted_pos.x += error.x;
        g_pred.predicted_pos.y += error.y;
        history_shift(&g_pred.history, error);
        g_pred.correction_accum.x += error.x;
        g_pred.correction_accum.y += error.y;
    }
    history_prune(&g_pred.history, snapshot_tick);
    adopt_authoritative_target();
}

/* Take the destination the server is actually walking to.
 *
 * The server re-plans one tick after the tap arrives, and it re-routes a blocked
 * target to the nearest walkable cell. Its route therefore trails the newest
 * command by a round trip, and adopting it too early walks the player back
 * toward where the previous tap was heading — the backward step and the reverse
 * flicker, at their worst when the player changes direction rapidly.
 *
 * Acknowledgement is the wrong signal for that: the server acks a command on
 * arrival, including one a later tap in the same tick supersedes. moveAck is
 * the right one — it names the command the route was planned for. Until it
 * covers the newest command the snapshot still describes the previous walk, and
 * the optimistic start that makes a tap feel immediate has to stand.
 *
 * The wait is bounded. A superseded command is never confirmed, so past the
 * deadline authority wins and the adopted target takes the sequence it was
 * planned for, which settles the state. */
static void adopt_authoritative_target(void) {
    if (g_pred.has_active &&
        session_last_movement_sequence() < g_pred.active.sequence &&
        GetTime() < g_pred.active_confirm_deadline) {
        return;
    }
    if (MODE_WALKING != g_game_state.player.base.mode) {
        g_pred.has_active = false; /* authority says the walk is over */
        path_clear();
        return;
    }
    g_pred.active.kind     = INPUT_KIND_PLAYER_ACTION;
    g_pred.active.target_x = g_game_state.player.target_pos.x;
    g_pred.active.target_y = g_game_state.player.target_pos.y;
    g_pred.active.sequence = session_last_movement_sequence();
    g_pred.has_active      = true;

    int count = g_game_state.player.path_count;
    if (0 > count) { count = 0; }
    if (MAX_PATH_POINTS < count) { count = MAX_PATH_POINTS; }
    for (int i = 0; i < count; i++) { g_pred.path[i] = g_game_state.player.path[i]; }
    g_pred.path_count = count;
    g_pred.path_index = 0;

    /* The route starts where the server stood at the snapshot tick, and this
     * client is already further along it. Drop the waypoints behind by
     * projection, not by distance: a waypoint is behind when the step to it
     * opposes the segment it belongs to. Picking the merely nearest waypoint
     * can select one the player has passed, and walking back to it is itself a
     * reversal. */
    while (g_pred.path_index + 1 < g_pred.path_count) {
        Vector2 head = g_pred.path[g_pred.path_index];
        Vector2 next = g_pred.path[g_pred.path_index + 1];
        float seg_x = next.x - head.x, seg_y = next.y - head.y;
        float to_head_x = head.x - g_pred.predicted_pos.x;
        float to_head_y = head.y - g_pred.predicted_pos.y;
        if (0.0f <= to_head_x * seg_x + to_head_y * seg_y) { break; }
        g_pred.path_index++;
    }
}

/* Raw predicted simulation position. Discrete at sim-tick granularity by
 * design — visual smoothing is the presentation layer's job
 * (domain/local_player_view). */
Vector2 prediction_self_position(void) { return g_pred.predicted_pos; }

Vector2 prediction_consume_correction(void) {
    Vector2 c = g_pred.correction_accum;
    g_pred.correction_accum = (Vector2){ 0.0f, 0.0f };
    return c;
}

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

void interpolation_compute_view(void) {
    const double now = GetTime();
    const int window_ms = g_game_state.interpolation_ms;

    for (int i = 0; i < g_game_state.other_player_count; i++) {
        PlayerState* p = &g_game_state.other_players[i];
        float t = compute_alpha_for(now, p->base.snapshot_time, window_ms);
        p->base.interp_pos.x = p->base.pos_prev.x + (p->base.pos_server.x - p->base.pos_prev.x) * t;
        p->base.interp_pos.y = p->base.pos_prev.y + (p->base.pos_server.y - p->base.pos_prev.y) * t;
    }
    for (int i = 0; i < g_game_state.bot_count; i++) {
        BotState* b = &g_game_state.bots[i];
        float t = compute_alpha_for(now, b->base.snapshot_time, window_ms);
        b->base.interp_pos.x = b->base.pos_prev.x + (b->base.pos_server.x - b->base.pos_prev.x) * t;
        b->base.interp_pos.y = b->base.pos_prev.y + (b->base.pos_server.y - b->base.pos_prev.y) * t;
    }
}
