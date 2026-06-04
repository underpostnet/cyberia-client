#ifndef CYBERIA_NETWORK_REPLICATION_H
#define CYBERIA_NETWORK_REPLICATION_H

#include "input/input.h"

/*
 * Client→server input replication.
 *
 * Drains the per-frame input event queue, builds typed input commands, applies
 * them to the prediction replay buffer, and ships them on the wire. This is the
 * single uplink path for player actions; it sits above the raw WebSocket I/O
 * owned by game_client and below the main loop that captures input.
 */

/* Drain a (deep-copied) frame input queue: per tap, build the command, enqueue
 * it for prediction, send it to the server, and set the local on-tap target. */
void replication_prepare_input(input_queue_t in_queue);

#endif /* CYBERIA_NETWORK_REPLICATION_H */
