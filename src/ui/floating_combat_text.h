#ifndef FLOATING_COMBAT_TEXT_H
#define FLOATING_COMBAT_TEXT_H

#include <stdbool.h>
#include <stdint.h>

/* Floating combat text — animated pop-up numbers at world positions, the
 * feedback channel for life changes. Coin and item changes go elsewhere: the
 * inventory-bar quantity FX shows coins, the loot grid shows item gains.
 *
 * Each entry pops (font scales up with a brief overshoot), rises with a
 * gentle random arc, then fades. Font size scales with log2(value + 1), so a
 * large hit reads heavier without overflowing the viewport.
 *
 * Draws inside BeginMode2D / EndMode2D, so entries follow the camera. */

/* Client-only: the wire carries the kind as a JSON string, so these numbers
 * never cross the network. message.c maps the server `kind` word
 * (game/snapshot.go: FCTDamage "damage", FCTRegen "regen") onto this enum.
 * To add a kind, add it in both places. */
typedef enum {
    FCT_TYPE_DAMAGE = 0,   /* life loss — red   "-N" */
    FCT_TYPE_REGEN  = 1,   /* life gain — green "+N" */
} FCTType;

#define FCT_MAX_ENTRIES 64

/* Call before any other fct_* function. Idempotent. */
void fct_init(void);

/* `value` is the magnitude, always positive or zero. */
void fct_spawn(float world_x, float world_y, uint32_t value, FCTType type);

/* Call once per frame, before fct_draw. */
void fct_update(float dt);

/* Call inside BeginMode2D / EndMode2D. */
void fct_draw(void);

/* Screen-space vignette: a red flash for damage, a green pulse for regen.
 * Call outside BeginMode2D, after EndMode2D. */
void fct_draw_overlay(void);

#endif /* FLOATING_COMBAT_TEXT_H */
