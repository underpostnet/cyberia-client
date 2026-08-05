/**
 * action_cache — client cache of CyberiaAction metadata fetched by code.
 *
 * The Go server sends only the bot's action CODE over AOI. The presentation
 * metadata — overhead label, greeting, the per-quest dialogue map that names
 * which quests the NPC handles, and the vendor catalog — is fetched lazily from
 * GET /api/cyberia-action/code/:code and cached here. Immutable, so each code
 * is fetched at most once per session.
 *
 * The catalog is presentation input only: the simulation owns the same rows
 * (delivered over gRPC) and is the sole authority on every purchase.
 */

#ifndef ACTION_CACHE_H
#define ACTION_CACHE_H

#include <stdbool.h>

#define ACTION_CACHE_CODE_MAX   64
#define ACTION_CACHE_LABEL_MAX  64
#define ACTION_CACHE_QUEST_MAX  8
#define ACTION_CACHE_SHOP_MAX   16
#define ACTION_CACHE_CAP  32

typedef enum {
    ACTION_CACHE_NONE = 0,
    ACTION_CACHE_LOADING,
    ACTION_CACHE_READY,
    ACTION_CACHE_ERROR,
} ActionCacheState;

typedef struct {
    char quest_code[ACTION_CACHE_CODE_MAX];
    char dialog_code[ACTION_CACHE_CODE_MAX];
} ActionQuestDlg;

typedef struct {
    char item_id[ACTION_CACHE_CODE_MAX];
    char price_item_id[ACTION_CACHE_CODE_MAX];
    int  price_qty;
} ActionShopItem;

typedef struct {
    char code[ACTION_CACHE_CODE_MAX];
    char label[ACTION_CACHE_LABEL_MAX];
    char dialog_code[ACTION_CACHE_CODE_MAX];
    char source_map_code[ACTION_CACHE_CODE_MAX];
    int  source_cell_x;
    int  source_cell_y;
    ActionQuestDlg quests[ACTION_CACHE_QUEST_MAX];
    int  quest_count;
    ActionShopItem shop_items[ACTION_CACHE_SHOP_MAX];
    int  shop_count;
    ActionCacheState state;
} ActionMetadataEntry;

void action_cache_reset(void);

/* Schedule an async REST fetch if not cached/loading. */
void action_cache_fetch(const char* code);

/* Cached action by code, or NULL. */
const ActionMetadataEntry* action_cache_get(const char* code);

#endif /* ACTION_CACHE_H */
