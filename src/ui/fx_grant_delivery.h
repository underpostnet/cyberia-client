#ifndef CYBERIA_UI_FX_GRANT_DELIVERY_H
#define CYBERIA_UI_FX_GRANT_DELIVERY_H

#include <raylib.h>
#include <stdbool.h>

/* Presentation sequence for a confirmed transaction the server must deliver —
 * a shop purchase, an assembler synthesis, anything that yields items.
 *
 * Each gained item's inventory-bar change is held until the authoritative grant
 * actually lands, then any spent items release their loss FX, and the gains fly
 * from the controls that produced them into their inventory slots, staggered so
 * a multi-output result reads as a sequence rather than a burst. Waiting is what
 * makes those flights aim at the real slots: a first copy has none until the
 * server delivers it.
 *
 * One sequence at a time — a modal transaction is exclusive by construction.
 *
 * Contract per frame: fx_grant_delivery_update(dt) once, after the inventory
 * bar has refreshed its quantity FX. */

#define FX_GRANT_GAINS_MAX 6
#define FX_GRANT_SPENT_MAX 8

/* One delivered item and the screen point it flies from. */
typedef struct {
    const char* item_id;
    Vector2     origin;
} FxGrantGain;

void fx_grant_delivery_init(void);
void fx_grant_delivery_reset(void);

/* Arm the sequence for a request whose grant is on its way. `spent_item_ids`
 * are released the moment the grant lands, so the loss reads before the
 * arrival; pass none when the spend already played (an assembly consumes its
 * ingredients when the progress bar starts, not when it finishes). */
void fx_grant_delivery_begin(const FxGrantGain* gains, int gain_count,
                             const char* const* spent_item_ids, int spent_count);

void fx_grant_delivery_update(float dt);

/* True until every gain has launched — the opener holds its card at rest for
 * this window. Also goes false when the transaction was rejected and the
 * sequence gave up. */
bool fx_grant_delivery_waiting(void);

#endif /* CYBERIA_UI_FX_GRANT_DELIVERY_H */
