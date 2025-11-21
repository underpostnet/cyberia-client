#ifndef CLIENT_H
#define CLIENT_H

// Client subsystem initialization
int client_init(void);

// Client main update (processes websocket events)
void client_update(void);

// Client cleanup
void client_cleanup(void);

// Check if client is connected
int client_is_connected(void);

// Send a message to the server
int client_send(const char* message);

// Get the last received message (for display)
const char* client_get_last_message(void);

#endif // CLIENT_H