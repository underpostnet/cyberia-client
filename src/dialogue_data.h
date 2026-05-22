/**
 * @file dialogue_data.h
 * @brief Async dialogue data fetcher and cache.
 *
 * Fetches dialogue records from the Engine API per item ID via the
 * engine_client fetch queue; completion arrives through a registered
 * callback (no per-frame poll required from the caller).
 *
 * API endpoint:
 *   GET {API_BASE_URL}/api/cyberia-dialogue/code/default-<itemId>
 *
 * Response shape (success):
 *   { "status": "success", "data": [ { code, order, speaker, text, mood }, ... ] }
 *
 * Usage:
 *   dialogue_data_init();
 *   dialogue_data_request("lain");          // fetches /code/default-lain
 *   const DialogueDataSet* d = dialogue_data_get("lain");
 *   if (d && d->state == DLG_DATA_READY) { ... use d->lines ... }
 */

#ifndef DIALOGUE_DATA_H
#define DIALOGUE_DATA_H

#include "ui/modal_dialogue.h"   /* DialogueLine */

#include <stdbool.h>

/* ── Fetch state ─────────────────────────────────────────────────────── */

typedef enum {
    DLG_DATA_NONE = 0,
    DLG_DATA_FETCHING,
    DLG_DATA_READY,
    DLG_DATA_EMPTY,     /* fetch succeeded but no records */
    DLG_DATA_ERROR
} DialogueDataState;

/**
 * @brief Cached dialogue data for one item ID.
 */
typedef struct {
    char            item_id[128];
    DialogueLine    lines[DIALOGUE_MAX_LINES];
    int             line_count;
    DialogueDataState state;
} DialogueDataSet;

/* ── Public API ──────────────────────────────────────────────────────── */

/** Initialise the dialogue data cache (call once at startup). */
void dialogue_data_init(void);

/** Destroy the cache and free memory. */
void dialogue_data_cleanup(void);

/**
 * @brief Request dialogue data for an item ID.
 *
 * If the data is already cached (any state), this is a no-op.
 * Otherwise kicks off an async HTTP fetch.
 */
void dialogue_data_request(const char* item_id);

/**
 * @brief Look up cached dialogue data for an item ID.
 *
 * @return Pointer to cached data, or NULL if not yet requested.
 */
const DialogueDataSet* dialogue_data_get(const char* item_id);

/**
 * @brief Check if an item ID has dialogue data available (READY with lines > 0).
 */
bool dialogue_data_available(const char* item_id);

#endif /* DIALOGUE_DATA_H */
