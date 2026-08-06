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

/* Confirmation callback for a quantity picker: the item and the count the
 * player settled on before pressing OK. */
typedef void (*ModalNotificationConfirmFn)(const char* item_id, int quantity);

/* Like modal_notification_show_reward, plus a ◀ / ▶ quantity stepper between
 * the slot and the buttons, a running total priced in `price_item_id`, and a
 * Cancel / Buy pair in place of the lone OK. `on_confirm` fires on Buy with the
 * selected count; Cancel dismisses with nothing sent.
 *
 * A picker announces a trade rather than a gift, so it drops the reward
 * celebration and waits for the server: after Buy it holds until the granted
 * items land in the inventory, releases the price item's spend FX, and only
 * then flies the item from its slot into the inventory slot. Passing the price
 * is what lets the spend animation play before the arrival instead of on the
 * generic fallback timer. */
void modal_notification_show_picker(const char* title, const char* message, Color accent,
                                    const char* item_id, int min_quantity, int max_quantity,
                                    const char* price_item_id, int price_quantity,
                                    ModalNotificationConfirmFn on_confirm);

/* One line of a result set: what is produced and how much of it. */
typedef struct {
    const char* item_id;
    int         quantity;
} ModalNotificationItem;

/* Cancellation callback for an assembly the player aborted mid-progress. */
typedef void (*ModalNotificationCancelFn)(void);

/* Assemble mode: a timed synthesis. The card stacks the consumed inputs over a
 * progress bar charging for `craft_seconds` over the produced outputs, with
 * fx_assemble's electric field converging on it, and a Cancel that aborts
 * before it completes.
 *
 * The inputs are consumed the moment the bar starts, so their inventory loss FX
 * releases right away; when the bar fills they spray out of their card slots
 * while the outputs fly from theirs into the inventory. */
typedef struct {
    const char*                  title;
    const char*                  message;
    Color                        accent;
    const ModalNotificationItem* inputs;
    int                          input_count;
    const ModalNotificationItem* outputs;
    int                          output_count;
    float                        craft_seconds;
    ModalNotificationCancelFn    on_cancel;
} ModalNotificationAssemble;

void modal_notification_show_assemble(const ModalNotificationAssemble* assemble);

/* Adopt the server's authoritative assembly duration once craft_ack lands.
 * Ignored when no assembly is running. */
void modal_notification_set_assemble_duration(float craft_seconds);

/* Abort a running assembly from outside (a rejected craft_ack). Closes the card
 * without calling the cancel callback — the server already knows. */
void modal_notification_abort_assemble(void);

void modal_notification_update(float dt);
void modal_notification_draw(void);

/* Handle a click on the notification. Returns true if consumed
 * (e.g. the OK button was tapped).  Call from the main click dispatch. */
bool modal_notification_handle_click(int mx, int my);

/* Returns true while a notification is visible. */
bool modal_notification_is_open(void);

/* Returns true during the post-close cooldown window. While this is active,
 * the click dispatch should swallow all taps to prevent accidental triggers
 * on elements that were behind the notification. */
bool modal_notification_is_on_cooldown(void);

#endif /* MODAL_NOTIFICATION_H */
