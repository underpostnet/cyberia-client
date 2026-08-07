#include "ui/fx_item_transfer.h"

#include "fx_inventory_bar_qty.h"
#include "item_slot.h"

#include <math.h>
#include <string.h>

#define FX_TRANSFER_MAX 8
#define FX_TRANSFER_DUR 0.90f
#define FX_TRANSFER_ARC_MIN 32.0f
#define FX_TRANSFER_ARC_MAX 96.0f

typedef struct {
    ObjectLayerState item;
    Rectangle        from;
    Rectangle        to;
    float            age;
    bool             inventory_arrival;
    bool             arrived;
    bool             live;
} FxTransfer;

static FxTransfer s_flights[FX_TRANSFER_MAX];

void fx_item_transfer_reset(void) {
    memset(s_flights, 0, sizeof(s_flights));
}

static void spawn(const ObjectLayerState* ols, Rectangle from, Rectangle to,
                  bool inventory_arrival) {
    if (!ols || '\0' == ols->item_id[0]) return;
    for (int i = 0; i < FX_TRANSFER_MAX; i++) {
        if (s_flights[i].live) continue;
        s_flights[i] = (FxTransfer){ .item = *ols, .from = from, .to = to,
                                     .age = 0.0f,
                                     .inventory_arrival = inventory_arrival,
                                     .live = true };
        if (inventory_arrival) fx_inventory_bar_qty_hold_for_delivery(ols->item_id);
        return;
    }
}

void fx_item_transfer_spawn(const ObjectLayerState* ols, Rectangle from, Rectangle to) {
    spawn(ols, from, to, false);
}

void fx_item_transfer_spawn_to_inventory(const ObjectLayerState* ols,
                                         Rectangle from, Rectangle to) {
    spawn(ols, from, to, true);
}

void fx_item_transfer_update(float dt) {
    for (int i = 0; i < FX_TRANSFER_MAX; i++) {
        if (!s_flights[i].live) continue;
        if (s_flights[i].arrived) {
            if (s_flights[i].inventory_arrival)
                fx_inventory_bar_qty_notify_arrival(s_flights[i].item.item_id);
            s_flights[i].live = false;
            continue;
        }
        if (s_flights[i].inventory_arrival)
            fx_inventory_bar_qty_hold_for_delivery(s_flights[i].item.item_id);
        s_flights[i].age += dt;
        if (FX_TRANSFER_DUR <= s_flights[i].age) {
            s_flights[i].age = FX_TRANSFER_DUR;
            s_flights[i].arrived = true;
        }
    }
}

void fx_item_transfer_draw(ObjectLayersManager* mgr) {
    for (int i = 0; i < FX_TRANSFER_MAX; i++) {
        if (!s_flights[i].live) continue;
        float t = s_flights[i].age / FX_TRANSFER_DUR;
        float e = t * t * (3.0f - 2.0f * t);
        Rectangle a = s_flights[i].from, b = s_flights[i].to;
        float dx = b.x - a.x;
        float dy = b.y - a.y;
        float arc = sqrtf(dx * dx + dy * dy) * 0.20f;
        if (FX_TRANSFER_ARC_MIN > arc) arc = FX_TRANSFER_ARC_MIN;
        if (arc > FX_TRANSFER_ARC_MAX) arc = FX_TRANSFER_ARC_MAX;
        Rectangle at = { a.x + dx * e,
                         a.y + dy * e - 4.0f * arc * t * (1.0f - t),
                         a.width + (b.width - a.width) * e,
                         a.height + (b.height - a.height) * e };
        item_slot_draw_ex(at, &s_flights[i].item, mgr, WHITE, 0.0f, true);
    }
}
