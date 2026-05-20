/**
 * @file domain/presentation_runtime.h
 * @brief Runtime hydration of presentation overrides from the engine REST
 *        endpoint /api/cyberia-client-hints/:code.
 *
 * The client owns its render policy. Built-in defaults live in
 * presentation_defaults.{c,h} and are sufficient for the client to run
 * end-to-end. This module adds optional, per-instance overrides:
 *
 *   - on startup, kick off an asynchronous GET against the engine,
 *   - on each render frame call presentation_runtime_poll() to advance
 *     the fetch state machine,
 *   - when the response arrives, merge override values on top of the
 *     built-in defaults so subsequent presentation_palette_lookup() and
 *     presentation_status_icon_*() calls reflect the per-instance values.
 *
 * Properties
 * ----------
 *   - Non-blocking: the client renders with built-in defaults from frame 0;
 *     overrides apply when (and only if) the fetch succeeds.
 *   - Idempotent: poll may be called every frame; once the fetch settles
 *     the module becomes a no-op.
 *   - Optional: a 404, network error, or missing endpoint is a silent
 *     fallback to defaults. Gameplay is unaffected.
 *   - Not authoritative: this module never affects simulation state.
 *     It only mutates a process-local override table consulted by the
 *     palette and status-icon lookup helpers.
 *
 * The Go simulation server never reads this endpoint.
 */

#ifndef CYBERIA_DOMAIN_PRESENTATION_RUNTIME_H
#define CYBERIA_DOMAIN_PRESENTATION_RUNTIME_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Kick off the asynchronous fetch. Safe to call exactly once at startup
 *  after js_init_engine_api has run. Subsequent calls are no-ops. */
void presentation_runtime_start_fetch(const char* api_base_url, const char* instance_code);

/** Advance the fetch state machine. Call once per render frame from the
 *  main loop. Returns true the first frame the override table becomes
 *  available (useful for one-shot logging / refresh hooks). */
bool presentation_runtime_poll(void);

/** True once the fetch has settled (succeeded, errored, or 404). After
 *  this point, all presentation lookups reflect the resolved table. */
bool presentation_runtime_is_ready(void);

/** Override-aware palette lookup. Falls back to the built-in defaults
 *  when no override exists for the key. */
struct Color;
typedef struct Color Color;
Color presentation_runtime_palette(const char* key);

/** Override-aware status-icon stem lookup. NULL if no icon for that id. */
const char* presentation_runtime_status_icon(unsigned char status_id);

/** Override-aware status-icon border colour. */
Color presentation_runtime_status_border(unsigned char status_id);

#ifdef __cplusplus
}
#endif

#endif /* CYBERIA_DOMAIN_PRESENTATION_RUNTIME_H */
