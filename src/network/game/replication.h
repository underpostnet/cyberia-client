#ifndef CYBERIA_NETWORK_REPLICATION_H
#define CYBERIA_NETWORK_REPLICATION_H

#include "network/game/client_event.h"
#include "domain/self_motion.h"

/* Prediction — the own player only. The live step and the replay move it
 * with one motion function; every other entity takes the server state. */
void prediction_reset(void);
/* The decoder hands over the self block of each snapshot. A newer state
 * replaces an older one that the reconcile did not apply yet. */
void prediction_on_snapshot(const self_motion_t* state, cyberia_frame_t frame,
                            cyberia_input_seq_t consumed);
/* One fixed step. s is the frame the step completes. */
void prediction_step(cyberia_frame_t s);
/* Phase 1 of the tick: replace with the newest state, then replay. */
void prediction_reconcile(void);
/* The last two fixed steps. The view draws between them. */
const self_motion_t* prediction_prev(void);
const self_motion_t* prediction_self(void);

/* Interpolation — render-time smoothing of remote entities (sole writer of
 * RenderState.interp_pos; never the local player). */
void interpolation_compute_view(void);

/* Session — per-connection frame and sequence counters (sole writer). */
void session_reset(void);
cyberia_input_seq_t session_next_input_sequence(void);
/* Completed fixed steps in this session. session_frame_advance is its only
 * writer, once per fixed step. */
cyberia_frame_t session_frame(void);
void            session_frame_advance(void);

#endif /* CYBERIA_NETWORK_REPLICATION_H */
