#ifndef CYBERIA_UI_SESSION_GATE_H
#define CYBERIA_UI_SESSION_GATE_H

#include <raylib.h>
#include <stdbool.h>

/* session_gate — the card that ends, refuses or switches a session: what
 * happened in plain words and the ways forward. A way opens the portal launch
 * bridge of this world as a guest or as the account, the world selection or
 * the sign-out, or a new connection. The session menu is the same card: the
 * player picks a way, the session ends with its final save, then the way
 * opens. While the card shows, it alone takes input. */

/* A session_end the server sent; `left` and `left_saving` finish a leave. */
void session_gate_end(const char* reason);

/* The socket closed without a session_end, and the session cannot resume. */
void session_gate_lost(void);

/* Opens the session menu. */
void session_gate_menu(void);

/* Marks the session an account session or a guest session. */
void session_gate_set_account(bool account);
bool session_gate_account(void);

bool session_gate_active(void);

/* True while a leave waits for the end of its session. */
bool session_gate_leaving(void);

/* The card, centred in `area`. */
void session_gate_draw(Rectangle area);
bool session_gate_handle_click(int mx, int my);

#endif /* CYBERIA_UI_SESSION_GATE_H */
