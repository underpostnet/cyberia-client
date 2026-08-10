#ifndef MODAL_ANCHOR_H
#define MODAL_ANCHOR_H

#include <raylib.h>
#include <stdbool.h>

/* Large-screen placement for the entity panel modals (interact, inventory
 * item detail).
 *
 * Below the breakpoint a panel modal is a full-width card filling the gap
 * between the top toolbar and the inventory bar. Above it there is more screen
 * than the card needs, so the same card becomes a compact rectangle parked over
 * the entity it describes — the modal reads as attached to its subject instead
 * of floating in the middle of the viewport.
 *
 * Only the card's screen rect changes: inner layout, tabs, controls and chrome
 * are computed from that rect exactly as before.
 */

/* Screen width (px) above which panel modals anchor over their target entity. */
#define MODAL_ANCHOR_MIN_SCREEN_W 1000

/* Anchored cards never grow past this, however wide the viewport gets. */
#define MODAL_ANCHOR_MAX_W 700.0f

/* Vertical gap between the entity's top edge and the card's bottom edge. */
#define MODAL_ANCHOR_GAP 12.0f

/* Vertical room a capture reserves below the corner it freezes. Without it a
 * card resolved against an entity low on the screen lands flush against the
 * bottom of the safe area, and the first content growth is absorbed by pushing
 * its top edge up — the opposite of growing downward from a fixed top. The
 * corner is therefore placed as if the card were this tall, so anything up to
 * this height extends downward without moving the top edge. */
#define MODAL_ANCHOR_GROWTH_RESERVE 460.0f

/* True when the viewport is large enough for the anchored layout. Mobile
 * viewports keep their own full-bleed layout regardless of width. */
bool modal_anchor_active(void);

/* Screen-space region a modal may occupy: the viewport minus the top toolbar
 * and the visible part of the inventory bar, inset by `pad`. Never returns a
 * height below `min_h`, so a squeezed viewport clamps instead of inverting. */
Rectangle modal_anchor_safe_area(float pad, float min_h);

/* A modal's frozen top-left corner in screen space.
 *
 * The corner is resolved from the entity while the modal opens and then held:
 * a card that chased its entity would slide out from under the pointer every
 * time the player walked. Because the corner — not the centre — is what is
 * held, a card whose content grows or shrinks does so downward from a fixed
 * top edge instead of shifting the whole panel. */
typedef struct {
    Vector2 top_left;
    bool    captured;
} ModalAnchor;

/* Resolve the corner from `entity_id`'s current screen position: a `size` card
 * centred on the entity and sitting `gap` above its top edge, fitted into
 * `safe`. Falls back to the centre of `safe` when the entity cannot be
 * projected. Call each frame while the modal opens, then stop — the last call
 * is the placement the card keeps. */
void modal_anchor_capture(ModalAnchor* anchor, const char* entity_id,
                          Vector2 size, float gap, Rectangle safe);

/* The card rect for a captured corner at the current `size`. The corner stays
 * put and the height extends downward from it; the result is re-fitted into
 * `safe` every frame, so neither a window resize nor a growing panel can push
 * it off screen. */
Rectangle modal_anchor_rect(const ModalAnchor* anchor, Vector2 size, Rectangle safe);

/* Ease an anchored card's height toward the height its content wants. A
 * negative `current` snaps (the first measurement of a session); otherwise the
 * height chases exponentially, so a tab switch or a late catalog fetch resizes
 * the card instead of jumping it. */
float modal_anchor_ease_height(float current, float target, float dt);

#endif /* MODAL_ANCHOR_H */
