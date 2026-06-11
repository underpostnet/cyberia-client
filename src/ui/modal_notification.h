/**
 * modal_notification — general-purpose transient notification toast.
 *
 * A small top-centre panel that announces an event (quest completed, reward
 * granted, …) then auto-dismisses after a few seconds. Purely informational:
 * it never blocks input. Uses the shared modal chrome for a consistent look.
 */

#ifndef MODAL_NOTIFICATION_H
#define MODAL_NOTIFICATION_H

#include <raylib.h>

void modal_notification_init(void);

/* Show a notification with a title line and a body line. accent tints the
 * left bar (e.g. green for a completed quest). */
void modal_notification_show(const char* title, const char* message, Color accent);

/* Like modal_notification_show but renders a reward item_slot (icon + quantity)
 * on the right — used to announce a granted reward by its item_id. */
void modal_notification_show_reward(const char* title, const char* message, Color accent,
                                    const char* reward_item_id, int reward_quantity);

void modal_notification_update(float dt);
void modal_notification_draw(void);

/* Handle a click on the notification. Returns true if consumed
 * (e.g. the OK button was tapped).  Call from the main click dispatch. */
bool modal_notification_handle_click(int mx, int my);

#endif /* MODAL_NOTIFICATION_H */
