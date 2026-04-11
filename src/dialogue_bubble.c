/**
 * @file dialogue_bubble.c
 * @brief Screen-space dialogue bubble column — 50×50 animated item icons
 *        stacked vertically on the left side of the screen.
 *
 * Each frame, scans all entities in the AOI (other players + bots that are
 * NOT skill/coin projectiles).  For each entity that has at least one active
 * ObjectLayer whose itemId has dialogue data available in the cache, one
 * bubble slot is created.
 *
 * The icon is rendered using ol_as_ico_draw (down_idle animation).
 * Clicking an icon opens the dialogue modal for that entity+item.
 */

#include "dialogue_bubble.h"
#include "dialogue_data.h"
#include "modal_dialogue.h"
#include "ol_as_animated_ico.h"
#include "game_state.h"
#include "client.h"
#include <raylib.h>
#include <string.h>
#include <stdio.h>

/* ── Module state ─────────────────────────────────────────────────────── */

static ObjectLayersManager* s_ol_mgr = NULL;
static DialogueBubbleSlot   s_slots[DBUBBLE_MAX_SLOTS];
static int                  s_slot_count = 0;

/* ── Colours ──────────────────────────────────────────────────────────── */

static const Color C_SLOT_BG     = {  18,  18,  30, 200 };
static const Color C_SLOT_BORDER = {  80,  80, 130, 200 };
static const Color C_SLOT_HOVER  = {  40,  50,  80, 220 };

/* ── Helpers ──────────────────────────────────────────────────────────── */

static Rectangle slot_rect(int index) {
    float x = (float)DBUBBLE_MARGIN_X;
    float y = (float)(DBUBBLE_MARGIN_Y + index * (DBUBBLE_ICON_SIZE + DBUBBLE_GAP));
    return (Rectangle){ x, y, (float)DBUBBLE_ICON_SIZE, (float)DBUBBLE_ICON_SIZE };
}

static bool hit_rect(int mx, int my, Rectangle r) {
    return ((float)mx >= r.x && (float)mx < r.x + r.width &&
            (float)my >= r.y && (float)my < r.y + r.height);
}

/** Mark an existing slot as still active, or add a new one.
 *  Returns true if successfully marked/added. */
static bool try_add_slot(const char* entity_id, const char* item_id) {
    if (!item_id || item_id[0] == '\0') return false;

    /* If this item_id is already tracked (any entity), just re-activate */
    for (int i = 0; i < s_slot_count; i++) {
        if (strcmp(s_slots[i].item_id, item_id) == 0) {
            s_slots[i].active = true;
            return true;
        }
    }

    if (s_slot_count >= DBUBBLE_MAX_SLOTS) return false;

    DialogueBubbleSlot* slot = &s_slots[s_slot_count];
    strncpy(slot->entity_id, entity_id, sizeof(slot->entity_id) - 1);
    slot->entity_id[sizeof(slot->entity_id) - 1] = '\0';
    strncpy(slot->item_id, item_id, sizeof(slot->item_id) - 1);
    slot->item_id[sizeof(slot->item_id) - 1] = '\0';
    slot->active = true;
    slot->appeared_at = GetTime();
    s_slot_count++;
    return true;
}

/**
 * Scan an entity's active ObjectLayers.  For each active layer whose itemId
 * has dialogue data available, add a bubble slot.  Also kick off a request
 * for any itemId not yet fetched.
 */
static void scan_entity(const char* entity_id, const EntityState* base) {
    if (!base || !entity_id || entity_id[0] == '\0') return;
    if (base->respawn_in > 0.0f) return; /* dead / ghost */

    for (int i = 0; i < base->object_layer_count; i++) {
        const ObjectLayerState* ol = &base->object_layers[i];
        if (!ol->active || ol->item_id[0] == '\0') continue;

        /* Kick off fetch if not yet requested */
        dialogue_data_request(ol->item_id);

        /* Only show bubble if data is ready */
        if (dialogue_data_available(ol->item_id)) {
            try_add_slot(entity_id, ol->item_id);
        }
    }
}

/* ── Public API ──────────────────────────────────────────────────────── */

void dialogue_bubble_init(ObjectLayersManager* ol_manager) {
    s_ol_mgr = ol_manager;
    s_slot_count = 0;
    memset(s_slots, 0, sizeof(s_slots));
}

void dialogue_bubble_update(void) {
    /* Mark all existing slots as stale; scan_entity will re-activate them */
    for (int i = 0; i < s_slot_count; i++)
        s_slots[i].active = false;

    /* Scan other players */
    for (int i = 0; i < g_game_state.other_player_count; i++) {
        scan_entity(
            g_game_state.other_players[i].base.id,
            &g_game_state.other_players[i].base
        );
    }

    /* Scan bots (skip skill / coin projectiles and doppelgangers) */
    for (int i = 0; i < g_game_state.bot_count; i++) {
        const BotState* bot = &g_game_state.bots[i];
        if (strcmp(bot->behavior, "skill") == 0) continue;
        if (strcmp(bot->behavior, "coin")  == 0) continue;
        if (bot->caster_id[0] != '\0')          continue; /* doppelganger */
        scan_entity(bot->base.id, &bot->base);
    }

    /* Remove stale slots whose minimum display time has elapsed */
    double now = GetTime();
    int write = 0;
    for (int read = 0; read < s_slot_count; read++) {
        bool keep = s_slots[read].active ||
                    (now - s_slots[read].appeared_at < DBUBBLE_MIN_DISPLAY_SEC);
        if (keep) {
            if (write != read)
                s_slots[write] = s_slots[read];
            write++;
        }
    }
    s_slot_count = write;
}

void dialogue_bubble_draw(void) {
    if (s_slot_count <= 0) return;

    int mx = GetMouseX();
    int my = GetMouseY();

    for (int i = 0; i < s_slot_count; i++) {
        Rectangle r = slot_rect(i);
        bool hovered = hit_rect(mx, my, r);

        /* Background */
        DrawRectangleRec(r, hovered ? C_SLOT_HOVER : C_SLOT_BG);
        DrawRectangleLinesEx(r, 1.5f, C_SLOT_BORDER);

        /* Animated icon (down_idle) */
        if (s_ol_mgr && s_slots[i].item_id[0] != '\0') {
            ol_as_ico_draw(s_ol_mgr, s_slots[i].item_id,
                           (int)r.x, (int)r.y, DBUBBLE_ICON_SIZE,
                           "down_idle", 0, WHITE);
        }
    }
}

bool dialogue_bubble_handle_click(int mx, int my, bool clicked) {
    if (s_slot_count <= 0 || !clicked) return false;

    for (int i = 0; i < s_slot_count; i++) {
        Rectangle r = slot_rect(i);
        if (hit_rect(mx, my, r)) {
            printf("[DIALOGUE_BUBBLE] Slot %d clicked: entity=%s item=%s\n",
                   i, s_slots[i].entity_id, s_slots[i].item_id);
            /* Open the dialogue modal for this entity+item */
            const DialogueDataSet* d = dialogue_data_get(s_slots[i].item_id);
            if (d && d->state == DLG_DATA_READY && d->line_count > 0) {
                modal_dialogue_open(
                    s_slots[i].entity_id,
                    s_slots[i].item_id,
                    d->lines,
                    d->line_count
                );
            } else {
                printf("[DIALOGUE_BUBBLE] No dialogue data ready for item=%s\n",
                       s_slots[i].item_id);
            }
            return true;
        }
    }
    return false;
}

int dialogue_bubble_slot_count(void) {
    return s_slot_count;
}
