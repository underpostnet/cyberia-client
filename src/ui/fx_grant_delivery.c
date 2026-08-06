#include "ui/fx_grant_delivery.h"

#include "fx_inventory_bar_qty.h"
#include "game_state.h"
#include "loot_fx.h"
#include "object_layer.h"

#include <string.h>

/* Give up on a grant that never lands (rejected or dropped request). */
#define FG_GRANT_TIMEOUT 3.0f
/* Gap between the spend FX and the first arrival, so the two read as cause and
 * effect rather than one blur. */
#define FG_SPEND_LEAD    0.25f
/* Gap between consecutive arrivals of a multi-output result. */
#define FG_STAGGER       0.18f

typedef struct {
    char    item_id[MAX_ITEM_ID_LENGTH];
    Vector2 origin;
    int     baseline;
    bool    launched;
} FgGain;

static struct {
    bool   waiting;
    FgGain gains[FX_GRANT_GAINS_MAX];
    int    gain_count;
    char   spent[FX_GRANT_SPENT_MAX][MAX_ITEM_ID_LENGTH];
    int    spent_count;
    float  age;
    float  spend_age; /* >= 0 once the grant landed and the spend FX fired */
} g_fg;

void fx_grant_delivery_init(void) {
    memset(&g_fg, 0, sizeof(g_fg));
}

void fx_grant_delivery_reset(void) {
    memset(&g_fg, 0, sizeof(g_fg));
}

void fx_grant_delivery_begin(const FxGrantGain* gains, int gain_count,
                             const char* const* spent_item_ids, int spent_count) {
    memset(&g_fg, 0, sizeof(g_fg));

    for (int i = 0; i < gain_count && g_fg.gain_count < FX_GRANT_GAINS_MAX; i++) {
        if (!gains[i].item_id || '\0' == gains[i].item_id[0]) continue;
        FgGain* g = &g_fg.gains[g_fg.gain_count++];
        strncpy(g->item_id, gains[i].item_id, MAX_ITEM_ID_LENGTH - 1);
        g->origin = gains[i].origin;
        g->baseline = game_state_item_quantity(g->item_id);
        fx_inventory_bar_qty_hold_for_delivery(g->item_id);
    }
    if (0 == g_fg.gain_count) return;

    for (int i = 0; i < spent_count && g_fg.spent_count < FX_GRANT_SPENT_MAX; i++) {
        if (!spent_item_ids[i] || '\0' == spent_item_ids[i][0]) continue;
        strncpy(g_fg.spent[g_fg.spent_count++], spent_item_ids[i], MAX_ITEM_ID_LENGTH - 1);
    }
    g_fg.spend_age = -1.0f;
    g_fg.waiting = true;
}

/* Releasing is a no-op once it has fired, so re-asserting every frame lands the
 * spend FX on the first frame the quantity FX has registered the debit — no
 * dependency on module update order. */
static void release_spent(void) {
    for (int i = 0; i < g_fg.spent_count; i++) {
        fx_inventory_bar_qty_notify_arrival(g_fg.spent[i]);
    }
}

/* Every gain has been credited by the authoritative snapshot. */
static bool grant_landed(void) {
    for (int i = 0; i < g_fg.gain_count; i++) {
        if (game_state_item_quantity(g_fg.gains[i].item_id) <= g_fg.gains[i].baseline) return false;
    }
    return true;
}

void fx_grant_delivery_update(float dt) {
    if (!g_fg.waiting) return;

    for (int i = 0; i < g_fg.gain_count; i++) {
        if (!g_fg.gains[i].launched) fx_inventory_bar_qty_hold_for_delivery(g_fg.gains[i].item_id);
    }
    g_fg.age += dt;

    if (g_fg.spend_age >= 0.0f) {
        release_spent();
        g_fg.spend_age += dt;
        int pending = 0;
        for (int i = 0; i < g_fg.gain_count; i++) {
            FgGain* g = &g_fg.gains[i];
            if (g->launched) continue;
            if (g_fg.spend_age >= FG_SPEND_LEAD + (float)i * FG_STAGGER) {
                loot_fx_reward_delivery(g->item_id, g->origin.x, g->origin.y);
                g->launched = true;
            } else {
                pending++;
            }
        }
        if (0 == pending) g_fg.waiting = false;
        return;
    }

    if (grant_landed()) {
        g_fg.spend_age = 0.0f;
        return;
    }
    if (g_fg.age >= FG_GRANT_TIMEOUT) {
        /* Rejected or dropped — release the holds with no change, so no popup. */
        for (int i = 0; i < g_fg.gain_count; i++) {
            fx_inventory_bar_qty_notify_arrival(g_fg.gains[i].item_id);
        }
        g_fg.waiting = false;
    }
}

bool fx_grant_delivery_waiting(void) {
    return g_fg.waiting;
}
