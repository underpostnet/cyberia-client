#ifndef CYBERIA_UI_SAVE_STATUS_H
#define CYBERIA_UI_SAVE_STATUS_H

#include "sim/local_player.h"

#include <stdbool.h>

/* save_status — the one line under the toolbar that tells an account player
 * how far its progress is saved, as the server reports it. It shows only what
 * the player can act on: a save slower than usual, a delay, or changes the
 * world cannot save yet; then, once, that the progress saved again. It also
 * greets a session that rejoined. It takes no input: taps reach the world. */

/* The durability of the newest snapshot. `unsaved_ms` is the age of the
 * oldest change the engine has not applied. */
void save_status_observe(Durability durability, int unsaved_ms);

/* An account session joined: `resumed` after a lost connection, `adopted`
 * when it plays changes its closed session had not saved yet. */
void save_status_joined(bool resumed, bool adopted);

/* The connection closed: what the line showed no longer holds. */
void save_status_reset(void);

/* The line, centred under `top`. */
void save_status_draw(int screen_width, float top);

#endif /* CYBERIA_UI_SAVE_STATUS_H */
