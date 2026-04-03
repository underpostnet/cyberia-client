/**
 * @file floating_combat_text.h
 * @brief General-purpose Floating Combat Text (FCT) module.
 *
 * Renders animated pop-up numbers at world positions.  Designed to be the
 * single visual feedback channel for all real-time economy and combat events:
 *
 *   FCT_TYPE_DAMAGE    — life lost  (red,    "-N")
 *   FCT_TYPE_REGEN     — life gained (green, "+N")
 *   FCT_TYPE_COIN_GAIN — coins received (yellow, "+N")
 *   FCT_TYPE_COIN_LOSS — coins lost / burned (yellow, "-N")
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
 *   render_update():   fct_update(delta_time);
 *   game_render_world()  (inside BeginMode2D): fct_draw();
 *   on server event:   fct_spawn(world_x, world_y, value, type);
 *
 * Binary wire format for server-sent events (14 bytes, little-endian):
 *   [0]      u8   0x04  (BIN_MSG_FCT)
 *   [1]      u8   fct_type  (one of FCT_TYPE_* below)
 *   [2..5]   f32  world_x
 *   [6..9]   f32  world_y
 *   [10..13] u32  value  (always positive; sign implied by type)
 */

#ifndef FLOATING_COMBAT_TEXT_H
#define FLOATING_COMBAT_TEXT_H

#include <stdint.h>
#include <stdbool.h>

/* ── FCT event types ─────────────────────────────────────────────────────
 * These values MUST stay in sync with Go constants in aoi_binary.go:
 *   FCTTypeDamage   = 0x00
 *   FCTTypeRegen    = 0x01
 *   FCTTypeCoinGain = 0x02
 *   FCTTypeCoinLoss = 0x03                                                  */
#define FCT_TYPE_DAMAGE    0x00   /* life loss    — red    "-N"            */
#define FCT_TYPE_REGEN     0x01   /* life gain    — green  "+N"            */
#define FCT_TYPE_COIN_GAIN 0x02   /* coins in     — yellow "+N"            */
#define FCT_TYPE_COIN_LOSS 0x03   /* coins out    — yellow "-N" (PvP/sink) */

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
 * @param type     One of FCT_TYPE_DAMAGE / FCT_TYPE_REGEN /
 *                 FCT_TYPE_COIN_GAIN / FCT_TYPE_COIN_LOSS.
 */
void fct_spawn(float world_x, float world_y, uint32_t value, uint8_t type);

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
