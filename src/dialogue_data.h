#ifndef DIALOGUE_DATA_H
#define DIALOGUE_DATA_H

#include "ui/modal_dialogue.h"   /* DialogueLine */

#include <stdbool.h>

/* Dialogue cache. Fetches records through the engine_client queue and
 * reports completion through a registered callback — the caller does not
 * poll. */

typedef enum {
    DLG_DATA_NONE = 0,
    DLG_DATA_FETCHING,
    DLG_DATA_READY,
    DLG_DATA_EMPTY,     /* fetch succeeded but no records */
    DLG_DATA_ERROR
} DialogueDataState;

typedef struct {
    char            item_id[128];
    DialogueLine    lines[DIALOGUE_MAX_LINES];
    int             line_count;
    DialogueDataState state;
} DialogueDataSet;

void dialogue_data_init(void);
void dialogue_data_cleanup(void);

/* Fetch GET /api/cyberia-dialogue/code/default-<item_id>. No-op when the
 * item is already cached in any state. */
void dialogue_data_request(const char* item_id);

/* Fetch GET /api/cyberia-dialogue/code/<code> verbatim — no "default-"
 * prefix. Cached under `code`; read it back with dialogue_data_get(code). */
void dialogue_data_request_code(const char* code);

/* Cached data, or NULL when the item was never requested. */
const DialogueDataSet* dialogue_data_get(const char* item_id);

/* True when the item is READY and holds at least one line. */
bool dialogue_data_available(const char* item_id);

#endif /* DIALOGUE_DATA_H */
