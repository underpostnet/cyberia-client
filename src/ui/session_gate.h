#ifndef CYBERIA_UI_SESSION_GATE_H
#define CYBERIA_UI_SESSION_GATE_H

#include <raylib.h>
#include <stdbool.h>

/* session_gate — the card that ends, refuses or switches a session: what
 * happened in plain words and the ways forward. It also leads a travel: the
 * save of the world left, then the entry of the target. A way opens the portal
 * launch bridge of this world or of another one, as a guest or as the account,
 * the world selection or the sign-out, or a new connection. The settings menu
 * is the same card: the sound and full-screen toggles act at once; for a way,
 * the session ends with its final save, then the way opens. While the card
 * shows, it alone takes input. */

/* A session_end the server sent; `left` and `left_saving` finish a leave.
 * `travel_ticket` is the ticket of the world a travel enters, or empty. */
void session_gate_end(const char* reason, const char* travel_ticket);

/* The socket closed without a session_end, and the session cannot resume. A
 * travel in this page then takes the launch bridge. */
void session_gate_lost(void);

/* The world a travel entered in this page sent init_data: the card closes. */
void session_gate_arrived(void);

/* Opens the settings menu. */
void session_gate_settings(void);
/* True while the settings menu shows. */
bool session_gate_settings_open(void);
/* Escape closes settings. Other session cards consume Escape. */
bool session_gate_handle_escape(void);

/* Leaves for another world of the universe: the session ends with its final
 * save, then the target opens with the same principal. With `in_place`, an
 * account plays the target in this page with the ticket the end of its session
 * carries; without it, or when that fails, the launch bridge of the target opens.
 * `path` is the path of the target on the client, `name` the name the card shows. */
void session_gate_travel(const char* instance_code, const char* path, const char* name, bool in_place);

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
