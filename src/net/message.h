#ifndef CYBERIA_NETWORK_MESSAGE_H
#define CYBERIA_NETWORK_MESSAGE_H

#include <stddef.h>
#include <stdint.h>

/* The one server-to-client dispatch point. Unpacks a frame, switches on the
 * envelope "type", and applies the payload to the game state. No other
 * module reads the wire. */
void message_receive(const uint8_t* data, size_t len);

/* Register a handler called when an init_data payload finishes. Keeps the
 * data flow outward: message.c signals interested modules instead of calling
 * into the network layer. `adopted` marks an account session that plays the
 * changes its closed session had not saved yet. */
typedef void (*MessageInitHandler)(bool adopted);
void message_set_init_handler(MessageInitHandler handler);

#endif // CYBERIA_NETWORK_MESSAGE_H
