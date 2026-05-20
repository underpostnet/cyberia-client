/**
 * @file domain/presentation_defaults.h
 * @brief Client-owned, compile-time presentation defaults.
 *
 * The cyberia-client is the authority for its own render policy.
 *
 * What lives here
 *   - The canonical named colour palette used as solid-colour fallbacks
 *     for entities that have no active ObjectLayer items, plus a few
 *     UI-only entries.
 *   - The canonical status-icon visual table (numeric ID → icon stem +
 *     border colour). The numeric IDs are shared with the simulation
 *     (they ride on the AOI wire); everything else here is presentation.
 *   - Camera, interpolation, and viewport tuning defaults.
 *   - The dev-overlay flag default.
 *
 * Where this stuff explicitly does NOT come from
 *   - The cyberia-server WS init payload. The server does not ship any
 *     palette or render-policy data.
 *   - The cyberia-server gRPC `InstanceConfig`. The simulation contract
 *     has none of this.
 *
 * Optional engine overrides
 *   The engine-cyberia process exposes /api/cyberia-client-hints/:code
 *   for per-instance overrides. Calling that endpoint is OPTIONAL; the
 *   client runs end-to-end with this file alone. When the endpoint is
 *   reachable, override values are merged on top of these defaults — see
 *   presentation_runtime.c.
 *
 * To add a new colour or status icon, edit:
 *   1. This file (canonical client-side defaults).
 *   2. engine/src/api/cyberia-client-hints/cyberia-presentation-hints.defaults.js
 *      (engine canonical defaults — kept in sync 1-to-1).
 */

#ifndef CYBERIA_DOMAIN_PRESENTATION_DEFAULTS_H
#define CYBERIA_DOMAIN_PRESENTATION_DEFAULTS_H

#include <raylib.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Palette entry ──────────────────────────────────────────────────── */
typedef struct {
    const char* key;
    Color       color;
} PresentationColorEntry;

extern const PresentationColorEntry kPresentationPalette[];
extern const int                    kPresentationPaletteCount;

/* ── Per-entity-type fallback colour key ───────────────────────────── */
typedef struct {
    const char* entity_type;
    const char* color_key;
} PresentationEntityColorKey;

extern const PresentationEntityColorKey kPresentationEntityColorKeys[];
extern const int                        kPresentationEntityColorKeyCount;

/* ── Status-icon presentation entry ────────────────────────────────── */
typedef struct {
    uint8_t     id;
    const char* icon_id;    /* NULL or empty → no icon */
    bool        bounce;
    Color       border_color;
} PresentationStatusIcon;

extern const PresentationStatusIcon kPresentationStatusIcons[];
extern const int                    kPresentationStatusIconCount;

/* ── Render tuning defaults ────────────────────────────────────────── */
#define PRESENTATION_CAMERA_SMOOTHING_DEFAULT           0.1f
#define PRESENTATION_CAMERA_ZOOM_DEFAULT                1.0f
#define PRESENTATION_WIDTH_SCREEN_FACTOR_DEFAULT        1.0f
#define PRESENTATION_HEIGHT_SCREEN_FACTOR_DEFAULT       1.0f
#define PRESENTATION_INTERPOLATION_MS_DEFAULT           100
#define PRESENTATION_DEV_UI_DEFAULT                     false

/* ── Lookup helpers ────────────────────────────────────────────────── */

/** Return the palette colour for `key`, or a neutral grey fallback when
 *  the key is unknown. */
Color presentation_palette_lookup(const char* key);

/** Return the canonical fallback colour for an entity_type string.
 *  Composes entity_type → color_key → palette. Falls back to neutral
 *  grey when nothing matches. */
Color presentation_entity_fallback_color(const char* entity_type);

/** Return the icon stem (e.g. "skull") for a u8 status ID, or NULL when
 *  no icon is configured for that ID. */
const char* presentation_status_icon_id(uint8_t status_id);

/** Return the border colour for a u8 status ID, or a neutral grey when
 *  the ID is unknown. */
Color presentation_status_icon_border(uint8_t status_id);

#ifdef __cplusplus
}
#endif

#endif /* CYBERIA_DOMAIN_PRESENTATION_DEFAULTS_H */
