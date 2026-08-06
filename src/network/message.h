#ifndef CYBERIA_NETWORK_MESSAGE_H
#define CYBERIA_NETWORK_MESSAGE_H

#include <stddef.h>
#include <stdint.h>

/**
 * @file message.h
 * @brief The single server → client dispatch point.
 *
 * message_receive unpacks one frame and switches on the envelope "type",
 * then applies the payload to the game state. No other module reads the
 * wire.
 */
void message_receive(const uint8_t* data, size_t len);

/* Register a handler called when an init_data payload finishes. Keeps the
 * data flow outward: message.c signals interested modules instead of calling
 * into the network layer. */
typedef void (*MessageInitHandler)(void);
void message_set_init_handler(MessageInitHandler handler);

/**
 * @brief Drop the prev-position snapshot used for interpolation.
 *
 * A new session must not resolve fresh UUIDs against the prior session's
 * positions, which would interpolate every entity from (0,0).
 */
void message_reset_prev_snapshots(void);

#endif // CYBERIA_NETWORK_MESSAGE_H
