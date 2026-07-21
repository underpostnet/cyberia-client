#ifndef CYBERIA_UI_LOOT_FX_H
#define CYBERIA_UI_LOOT_FX_H

#include <stdbool.h>
#include <stdint.h>

#include "object_layer.h"
#include "world_types.h"

/* Loot drop FX — the full client-side visual lifecycle of a scattered drop.
 *
 * Presentation-only: the simulation never reads this module. It interpolates
 * authoritative server events into a three-stage cinematic:
 *
 *   1. Launch  (MsgTypeDropSpawn) — the in-world drop token arcs from the
 *      corpse center to its rest cell with a swift parabola and a landing
 *      bounce. Collision is server-gated for the same window.
 *   2. Idle    — while it waits, the token floats on a continuous sine bounce
 *      over its cell (no background plate).
 *   3. Collect (MsgTypeDropCollect) — a particle burst fires at the ground
 *      location (gold when the local player was eligible for the drop, gray
 *      when it was another player's loot) and the token vacuums along a
 *      parabolic arc into the collecting player's avatar boundary before it
 *      is erased.
 *
 * Stages 1–2 drive the render position of the drop bot that lives in the world
 * mirror (loot_fx_drop_render_pos). Stage 3 owns detached flight tokens and the
 * particle field, painted through the renderer bridge below. */

typedef struct {
    char  item_id[MAX_ITEM_ID_LENGTH];
    float x, y;   /* current token center, grid units          */
    float size;   /* current square side, grid units           */
    float alpha;  /* 0..1 — fades to 0 as it enters the UI      */
} LootFxRender;

/* Particle tint — personal per player: gold for loot the local player may
 * collect (damage contributor), gray for another player's loot. */
enum {
    LOOT_FX_TINT_GOLD = 0,
    LOOT_FX_TINT_GRAY = 1,
};

typedef struct {
    float   x, y;    /* center, grid units                        */
    float   size;    /* square side, grid units                   */
    float   alpha;   /* 0..1                                       */
    uint8_t tint;    /* LOOT_FX_TINT_*                             */
} LootFxParticle;

/* Reset all flights, drop animations, and particles (call on world reset). */
void loot_fx_reset(void);

/* MsgTypeDropSpawn: register a corpse→cell launch for an in-world drop token.
 * origin/landing are grid-unit centers; launch_ms is the settle duration. */
void loot_fx_note_spawn(const char* drop_id, float origin_x, float origin_y,
                        float landing_x, float landing_y, const char* item_id,
                        uint16_t launch_ms);

/* MsgTypeDropCollect: fire the burst + vacuum flight and stop the in-world idle
 * render for this token. world_x / world_y are the token's ground center. */
void loot_fx_push(const char* drop_id, const char* collector_id,
                  const char* item_id, float world_x, float world_y);

/* Advance every flight, drop animation, and particle by dt seconds. */
void loot_fx_update(float dt);

/* In-world drop render hook. Given the token's authoritative AOI rect (top-left
 * + dims, grid units), writes the animated top-left draw position (launch arc
 * or idle float). `loot_eligible` is the per-viewer AOI flag
 * (INTERACTION_FLAG_LOOT_ELIGIBLE) selecting the particle tint. Returns false
 * when the token has been collected and its in-world render must yield to the
 * detached vacuum flight. */
bool loot_fx_drop_render_pos(const char* drop_id,
                             float land_topx, float land_topy,
                             float dims_w, float dims_h,
                             bool loot_eligible,
                             float* out_topx, float* out_topy);

/* True while a pickup of this item is still travelling to the local player's
 * inventory (vacuum flight toward the local player, or a slot-delivery stream
 * whose arrival burst has not fired yet). The inventory bar defers revealing a
 * first-copy slot until this turns false. */
bool loot_fx_inbound_to_inventory(const char* item_id);

/* Renderer bridge: detached vacuum tokens. */
int  loot_fx_slot_count(void);
bool loot_fx_render_at(int i, LootFxRender* out);

/* Renderer bridge: collection-burst particles (world-space). */
int  loot_fx_particle_slot_count(void);
bool loot_fx_particle_at(int i, LootFxParticle* out);

/* Renderer bridge: screen-space delivery particles (inventory slot arrival). */
typedef struct {
    float   x, y;    /* center, screen pixels                    */
    float   size;    /* square side, screen pixels               */
    float   alpha;   /* 0..1                                     */
    uint8_t tint;    /* LOOT_FX_TINT_* (always gold: local-only)  */
} LootFxScreenParticle;

int  loot_fx_screen_particle_slot_count(void);
bool loot_fx_screen_particle_at(int i, LootFxScreenParticle* out);

/* Renderer bridge: the delivery token — the picked item's ObjectLayer icon
 * riding the same avatar→slot arc as the delivery stream, slightly above the
 * particles, so the item itself is seen entering the inventory. */
typedef struct {
    char  item_id[MAX_ITEM_ID_LENGTH];
    float x, y;      /* center, screen pixels                    */
    float size;      /* square side, screen pixels               */
    float alpha;     /* 0..1                                      */
} LootFxDeliveryToken;

int  loot_fx_delivery_token_slot_count(void);
bool loot_fx_delivery_token_at(int i, LootFxDeliveryToken* out);

/* Fly a reward item from an arbitrary screen position (e.g. the notification
 * modal's reward slot) into its inventory slot, reusing the pickup delivery
 * presentation: homing stream + delivery token + slot-arrival burst, which
 * releases the held +N popup / first-copy reveal on landing. */
void loot_fx_reward_delivery(const char* item_id, float from_x, float from_y);

/* Reverse of delivery: on a quantity reduction (coins lost on death, a quest
 * step "collect" turn-in, any item-slot decrement) the item icon and a particle
 * spray fly OUT of the slot to random screen points along a parabola, then fade.
 * Starts at the item's inventory slot, or the bottom-left toggle when the bar is
 * hidden. Paired with the fx_inventory_bar_qty "-N" popup on the same event. */
void loot_fx_slot_expend(const char* item_id);

/* Same outward spray from an explicit screen origin — used when the item's slot
 * is already gone (its last copy was consumed) so the caller supplies the slot's
 * last-known center. */
void loot_fx_slot_expend_at(const char* item_id, float from_x, float from_y);

#endif /* CYBERIA_UI_LOOT_FX_H */
