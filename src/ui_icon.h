/**
 * @file ui_icon.h
 * @brief General-purpose UI Icon System for Cyberia Online.
 *
 * Lightweight, reusable module for rendering small PNG icons fetched from the
 * engine API.  Designed for overhead entity indicators (status/mood/behavior)
 * but usable anywhere a dynamic icon is needed.
 *
 * ── Industry context ────────────────────────────────────────────────────
 * "Overhead Status Indicators" (OSIs) are the standard MMORPG mechanism for
 * conveying entity mood, aggro state, quest availability, and special status
 * at a glance without opening a tooltip.  Examples:
 *   - World of Warcraft: yellow "!" / grey "?" above NPCs (quest state)
 *   - Final Fantasy XIV: red/yellow icons over enemies (aggro/claim state)
 *   - Ultima Online: skull icon for murderers, shield for invulnerables
 *
 * This module provides the texture cache and animated renderer; the
 * entity_status module decides WHICH icon to show for a given entity.
 *
 * ── Asset pipeline ──────────────────────────────────────────────────────
 * Icons live on the engine API under:
 *   {API_BASE_URL}/assets/ui-icons/{icon_id}.png
 *
 * Textures are loaded asynchronously via the WASM JS bridge
 * (js_start_fetch_binary / js_get_fetch_result).  Until loaded, a subtle
 * pulsing placeholder dot is drawn so layout stays stable.
 *
 * ── Animation ───────────────────────────────────────────────────────────
 * Active icons use a smooth ease-in-out "hover float" animation (sine
 * wave, configurable amplitude, ~0.8 Hz) to draw attention without being
 * distracting.  The bounce phase is seeded from an entity ID hash so
 * nearby icons don't move in lockstep.
 */

#ifndef UI_ICON_H
#define UI_ICON_H

#include <raylib.h>
#include <stdbool.h>

/* ── Configuration ───────────────────────────────────────────────────── */

/**
 * Default rendered size of an overhead status icon (pixels).
 * 28 px is large enough to be clearly visible at typical zoom levels
 * without competing with the nameplate or HP bar.
 */
#define UI_ICON_DEFAULT_SIZE    28

/**
 * Bounce amplitude in pixels (half of total vertical travel).
 * 2.5 px gives a gentle, premium-feeling float that's noticeable
 * but never distracting.
 */
#define UI_ICON_BOUNCE_AMP      2.5f

/**
 * Bounce frequency in Hz.  0.8 Hz ≈ one full cycle every 1.25 seconds,
 * slow enough to feel "floaty" and organic rather than frantic.
 */
#define UI_ICON_BOUNCE_FREQ     1.1f

/* ── Public API ─────────────────────────────────────────────────────── */

/**
 * @brief Initialise the UI icon system (internal cache + request IDs).
 *
 * Must be called once during game_render_init().
 */
void ui_icon_init(void);

/**
 * @brief Poll all in-flight icon texture fetches.
 *
 * Call once per frame (e.g. from game_render_frame or game_render_ui)
 * to convert completed WASM fetches into GPU textures.
 */
void ui_icon_poll(void);

/**
 * @brief Release all cached icon textures and free memory.
 *
 * Call during game_render_cleanup().
 */
void ui_icon_cleanup(void);

/**
 * @brief Draw an icon at the given world-pixel position.
 *
 * The icon is fetched (and cached) from:
 *   {API_BASE_URL}/assets/ui-icons/{icon_id}.png
 *
 * While the texture is loading, a subtle pulsing placeholder dot is drawn
 * so the layout stays stable.
 *
 * @param icon_id   Filename stem (e.g. "arrow-down-red", "skull").
 *                  NULL or "" = draw nothing.
 * @param cx        Centre X in world pixels (inside BeginMode2D).
 * @param cy        Centre Y in world pixels (before bounce offset).
 * @param size      Rendered width & height (square) in pixels.
 * @param bounce    If true, apply the smooth hover-float animation.
 * @param phase     Phase offset in radians (use entity ID hash for desync).
 */
void ui_icon_draw(const char* icon_id, float cx, float cy,
                  int size, bool bounce, float phase);

#endif /* UI_ICON_H */
