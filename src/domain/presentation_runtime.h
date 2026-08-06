#ifndef CYBERIA_DOMAIN_PRESENTATION_RUNTIME_H
#define CYBERIA_DOMAIN_PRESENTATION_RUNTIME_H

#include <stdbool.h>
#include <stdint.h>
#include <raylib.h>

/* Sole owner of the presentation surface. The client holds no compile-time
 * palette, status-icon table, or camera tuning: every value arrives from
 * GET /api/cyberia-client-hints/:CYBERIA_CLIENT_HINTS_CODE.
 *
 * main() starts the async fetch once, after js_init_engine_api(). The
 * engine_client callback parses the palette, the entity colour keys, the
 * status-icon visuals, and the camera and cell tunings; it hydrates
 * cell_size and interpolation_ms into GameState. Renderers and UI read the
 * accessors below; domain/camera.c reads the zoom on demand.
 *
 * Until the fetch settles the accessors return a small inline bootstrap
 * value, so the splash screen has something to draw. Those are not a real
 * palette.
 *
 * Presentation only: the Go server never serves this endpoint, and gameplay
 * state never touches this module. */

#ifdef __cplusplus
extern "C" {
#endif

/* Start the fetch. Call once at startup, after js_init_engine_api. Later
 * calls are no-ops. */
void presentation_runtime_start_fetch(const char* client_hints_code);

/* True once the fetch settles — success, error, or 404. The accessors give
 * resolved values after that point, bootstrap values before it. */
bool presentation_runtime_is_ready(void);

/* ── Presentation accessors ────────────────────────────────────────── */

/* Palette colour for `key`, or a neutral grey when unknown. */
Color presentation_runtime_palette(const char* key);

/* Per-entity-type fallback colour. Composes entity_type → color_key →
 * palette. Falls back to neutral grey when nothing matches. */
Color presentation_runtime_entity_fallback_color(const char* entity_type);

/* Icon stem (e.g. "skull") for a u8 status ID, or NULL when no icon. */
const char* presentation_runtime_status_icon(uint8_t status_id);

/* Border colour for a u8 status ID, or neutral grey when unknown. */
Color presentation_runtime_status_border(uint8_t status_id);

/* Camera and cell sizing. */
float    presentation_runtime_cell_size(void);
float    presentation_runtime_camera_zoom(void);
float    presentation_runtime_camera_smoothing(void);
int      presentation_runtime_interpolation_ms(void);
float    presentation_runtime_default_obj_width(void);
float    presentation_runtime_default_obj_height(void);
bool     presentation_runtime_dev_ui(void);
void     presentation_runtime_set_dev_ui(bool enabled);
void     presentation_runtime_toggle_dev_ui(void);

/* Main UI font: TTF file name under engine assets/fonts/ ("" = built-in
 * font) and a multiplier applied to every text size. */
const char* presentation_runtime_font_family(void);
float       presentation_runtime_font_factor_size(void);

#ifdef __cplusplus
}
#endif

#endif /* CYBERIA_DOMAIN_PRESENTATION_RUNTIME_H */
