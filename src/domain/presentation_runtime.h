/**
 * @file domain/presentation_runtime.h
 * @brief Sole owner of the cyberia-client presentation surface.
 *
 * Architecture
 * ------------
 * The cyberia-client carries no hardcoded palette, no compile-time status
 * icon table, no compile-time camera tunings. Every presentation value
 * comes from the engine REST endpoint
 *
 *     GET /api/cyberia-client-hints/:CYBERIA_CLIENT_HINTS_CODE
 *
 * Lifecycle
 * ---------
 *   1. main() calls presentation_runtime_start_fetch() once after
 *      js_init_engine_api() — kicks off the async GET.
 *   2. main_loop() calls presentation_runtime_poll() every frame —
 *      advances the fetch state machine.
 *   3. When the response settles, the runtime parses palette, entity
 *      colour keys, status-icon visuals, and camera/cell tunings, then
 *      hydrates g_game_state.cell_size, g_game_state.interpolation_ms,
 *      and g_game_state.camera.zoom (one-shot write).
 *   4. Renderers / UI code consult the runtime accessors below.
 *
 * Bootstrap fallback
 * ------------------
 * A tiny inline palette + neutral grey is hardcoded inside the .c so the
 * splash screen has *something* to draw before the fetch settles.  These
 * are NOT a real palette — they exist only to avoid a black screen during
 * the few frames between window-up and fetch-complete.
 *
 * Strict boundary
 * ---------------
 * The cyberia-server (Go) never serves this endpoint.  The data here is
 * presentation only — gameplay state never touches this module.
 */

#ifndef CYBERIA_DOMAIN_PRESENTATION_RUNTIME_H
#define CYBERIA_DOMAIN_PRESENTATION_RUNTIME_H

#include <stdbool.h>
#include <stdint.h>
#include <raylib.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Kick off the asynchronous fetch. Safe to call exactly once at startup
 *  after js_init_engine_api has run. Subsequent calls are no-ops. */
void presentation_runtime_start_fetch(const char* api_base_url, const char* client_hints_code);

/** Advance the fetch state machine. Call once per render frame from the
 *  main loop. Returns true the first frame the response settles
 *  (succeeded, errored, or 404) — useful for one-shot hydration hooks. */
bool presentation_runtime_poll(void);

/** True once the fetch has settled (succeeded, errored, or 404). After
 *  this point, all accessors reflect the resolved table; before then they
 *  return the inline bootstrap values. */
bool presentation_runtime_is_ready(void);

/* ── Presentation accessors ────────────────────────────────────────── */

/** Palette colour for `key`, or a neutral grey when unknown. */
Color presentation_runtime_palette(const char* key);

/** Per-entity-type fallback colour. Composes entity_type → color_key →
 *  palette. Falls back to neutral grey when nothing matches. */
Color presentation_runtime_entity_fallback_color(const char* entity_type);

/** Icon stem (e.g. "skull") for a u8 status ID, or NULL when no icon. */
const char* presentation_runtime_status_icon(uint8_t status_id);

/** Border colour for a u8 status ID, or neutral grey when unknown. */
Color presentation_runtime_status_border(uint8_t status_id);

/** Camera and cell-sizing accessors. Defined values once the fetch
 *  settles; bootstrap values before that. */
float    presentation_runtime_cell_size(void);
float    presentation_runtime_camera_zoom(void);
float    presentation_runtime_camera_smoothing(void);
int      presentation_runtime_interpolation_ms(void);
float    presentation_runtime_default_obj_width(void);
float    presentation_runtime_default_obj_height(void);
bool     presentation_runtime_dev_ui(void);

#ifdef __cplusplus
}
#endif

#endif /* CYBERIA_DOMAIN_PRESENTATION_RUNTIME_H */
