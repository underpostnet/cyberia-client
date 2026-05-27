#include "session.h"
#include <raylib.h>
#include "config.h"

/* Singleton session state. Owned by this translation unit; readers go
 * through the accessor functions in session.h. */
static struct {
    cyberia_tick_t       last_server_tick;
    cyberia_input_seq_t  last_acked_input_sequence;
    cyberia_input_seq_t  next_input_sequence;
    double               last_snapshot_wall_time; /* GetTime() when snapshot arrived */
} g_sess = {0};

void session_on_snapshot(uint32_t snapshot_tick, uint32_t last_acked_sequence) {
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
}

cyberia_tick_t session_last_server_tick(void) {
    return g_sess.last_server_tick;
}

cyberia_input_seq_t session_last_acked_input_sequence(void) {
    return g_sess.last_acked_input_sequence;
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
    if (est <= INTERP_TICKS) return 0;
    return est - INTERP_TICKS;
}

cyberia_input_seq_t session_next_input_sequence(void) {
    return ++g_sess.next_input_sequence;
}
