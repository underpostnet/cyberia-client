/**
 * @file floating_combat_text.h
 * @brief General-purpose Floating Combat Text (FCT) module.
 *
 * Renders animated pop-up numbers at world positions. It is the visual
 * feedback channel for real-time combat events:
 *
 *   FCT_TYPE_DAMAGE — life lost   (red,   "-N")
 *   FCT_TYPE_REGEN  — life gained (green, "+N")
 *
 * Coin and item changes do NOT come through here — the inventory-bar quantity
 * FX shows coins, and the loot grid shows item gains.
 *
 * Each entry animates through three phases:
 *   1. Pop    (0 → FCT_POP_DURATION): font scales up with a brief overshoot.
 *   2. Rise   (pop → FCT_FADE_START): drifts upward + gentle random arc.
 *   3. Fade   (FCT_FADE_START → end): alpha decays to 0, vertical rise slows.
 *
 * Font size scales with log₂(value + 1) so +1000 damage feels heavier than
 * +5 regen without numbers overflowing the viewport.
 *
 * The module draws entirely inside a BeginMode2D / EndMode2D block, so
 * positions follow the world camera automatically.
 *
 * Typical integration:
 *   render_on_tick():   fct_update(delta_time);
 *   game_render_world()  (inside BeginMode2D): fct_draw();
 *   on server event:   fct_spawn(world_x, world_y, value, type);
 *
 * The server sends a `combat_text` JSON message: kind, worldX, worldY, value.
 * message.c maps the kind word to an FCTType below.
 */

#ifndef FLOATING_COMBAT_TEXT_H
#define FLOATING_COMBAT_TEXT_H

#include <stdbool.h>
#include <stdint.h>

/* ── FCT event types ─────────────────────────────────────────────────────
 * Client-only. The wire carries the kind as a JSON string, so these numbers
 * never cross the network and need no server counterpart. message.c maps the
 * server's `kind` word (game/snapshot.go: FCTDamage "damage", FCTRegen
 * "regen") onto this enum. To add a kind, add it in both places.            */
typedef enum {
    FCT_TYPE_DAMAGE = 0,   /* life loss — red   "-N" */
    FCT_TYPE_REGEN  = 1,   /* life gain — green "+N" */
} FCTType;

/** Maximum number of concurrently active FCT entries. */
#define FCT_MAX_ENTRIES 64

/* ── Public API ──────────────────────────────────────────────────────── */

/**
 * @brief Initialise the FCT pool.
 * Must be called before any other fct_* function (safe to call multiple times).
 */
void fct_init(void);

/**
 * @brief Spawn a new Floating Combat Text entry at a world position.
 *
 * @param world_x  World-space X coordinate (same unit as entity positions).
 * @param world_y  World-space Y coordinate.
 * @param value    Magnitude of the event (always non-negative).
 * @param type     FCT_TYPE_DAMAGE or FCT_TYPE_REGEN.
 */
void fct_spawn(float world_x, float world_y, uint32_t value, FCTType type);

/**
 * @brief Advance all active FCT entries by dt seconds.
 * Call once per frame, before fct_draw().
 */
void fct_update(float dt);

/**
 * @brief Draw all active FCT entries.
 * Must be called inside a BeginMode2D / EndMode2D block.
 */
void fct_draw(void);

/**
 * @brief Draw screen-space vignette overlays for damage and regen events.
 * Must be called OUTSIDE BeginMode2D (in screen space), after EndMode2D.
 * Damage spawns a brief red flash; regen spawns a brief green pulse.
 */
void fct_draw_overlay(void);

#endif /* FLOATING_COMBAT_TEXT_H */
