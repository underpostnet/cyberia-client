#include "fx/loot_fx.h"
#include "audio/audio_events.h"
#include "domain/local_player.h"
#include "game_state.h"

#include <assert.h>
#include <math.h>
#include <raylib.h>
#include <stdio.h>
#include <string.h>

GameState g_game_state;
LocalPlayer g_local_player;
static int pickup_cues;
static int slot_deliveries;

void audio_event(const char* logic_event_id) {
    if (0 == strcmp(AUDIO_EVENT_ITEM_PICKUP, logic_event_id)) pickup_cues++;
}
bool inventory_bar_item_slot_center(const char* item_id, Vector2* out) {
    slot_deliveries++;
    *out = (Vector2){ 100.0f, 100.0f };
    return true;
}
Vector2 game_render_world_to_screen(Vector2 world_pos) { return world_pos; }
int game_state_slot(EntityID id) { return 0; }
void fx_inventory_bar_qty_notify_arrival(const char* item_id) {}
int GetScreenWidth(void) { return 800; }
int GetScreenHeight(void) { return 600; }

/* The idle float keeps x on the cell and lifts y by at most the hover and the bounce. */
static void assert_idle_on(EntityID drop_id, float topx, float topy) {
    float x = 0.0f, y = 0.0f;
    assert(loot_fx_drop_render_pos(drop_id, topx, topy, 1.6f, 1.6f, true, &x, &y));
    assert(x == topx);
    assert(fabsf(y - topy) <= 0.31f);
}

/* Collects the token at (5, 5) for the local player and lets the flight land. */
static void collect(EntityID drop_id, bool granted) {
    pickup_cues = 0;
    slot_deliveries = 0;
    loot_fx_push(drop_id, g_local_player.id, "gel", 5.8f, 5.8f, granted);
    float x, y;
    assert(!loot_fx_drop_render_pos(drop_id, 5.0f, 5.0f, 1.6f, 1.6f, true, &x, &y));
    assert(granted == loot_fx_inbound_to_inventory("gel"));
    loot_fx_update(0.6f);
    assert(!loot_fx_inbound_to_inventory("gel") || granted);
}

int main(void) {
    g_local_player.id = 7;
    g_game_state.mask[0] = COMP_TRANSFORM;
    loot_fx_reset();

    /* A world token arrives by replication only: it idles on its own cell. */
    assert_idle_on(1, 5.0f, 5.0f);
    loot_fx_update(0.25f);
    assert_idle_on(1, 5.0f, 5.0f);

    /* A zero-length launch idles at once on its landing cell, with no arc. */
    loot_fx_note_spawn(2, 5.8f, 5.8f, 5.8f, 5.8f, "gel", 0);
    assert_idle_on(2, 5.0f, 5.0f);

    /* A death drop still flies from the corpse, then idles on its cell. */
    loot_fx_note_spawn(3, 1.8f, 1.8f, 5.8f, 5.8f, "gel", 420);
    float x, y;
    assert(loot_fx_drop_render_pos(3, 5.0f, 5.0f, 1.6f, 1.6f, true, &x, &y));
    assert(fabsf(x - 1.0f) < 0.01f);
    loot_fx_update(0.5f);
    assert_idle_on(3, 5.0f, 5.0f);

    /* A heal flies to the collector and delivers nothing to the inventory. */
    collect(1, false);
    assert(0 == pickup_cues && 0 == slot_deliveries);

    /* An item still lands in its inventory slot with the pickup cue. */
    collect(3, true);
    assert(1 == pickup_cues && 1 == slot_deliveries);

    /* A stale collection of a token never seen, or of no item, is safe. */
    loot_fx_push(99, 8, "gel", 1.0f, 1.0f, true);
    loot_fx_push(99, 8, "", 1.0f, 1.0f, true);
    loot_fx_update(1.0f);
    loot_fx_reset();
    printf("loot_fx_test OK\n");
    return 0;
}
