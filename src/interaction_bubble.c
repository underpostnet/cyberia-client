/**
 * @file interaction_bubble.c
 * @brief Entity-specific interaction bubbles — full OL-stack icons in a
 *        screen-space column on the left side.
 *
 * Each bubble represents one interactable entity in the player's AOI.
 * The icon is the entity's full active ObjectLayer stack rendered at
 * icon size, so players recognise entities at a glance.
 *
 * Tapping a bubble opens the JS social overlay (via social_bridge.h)
 * and sends freeze_start("social") to protect the player.
 */

#include "interaction_bubble.h"
#include "social_bridge.h"
#include "dialogue_data.h"
#include "ol_as_animated_ico.h"
#include "game_state.h"
#include "entity_render.h"
#include "game_render.h"
#include "client.h"
#include <raylib.h>
#include <string.h>
#include <stdio.h>

/* ── Module state ─────────────────────────────────────────────────────── */

static InteractionBubbleSlot s_slots[IBUBBLE_MAX_SLOTS];
static int                   s_slot_count = 0;

/* ── Colours ──────────────────────────────────────────────────────────── */

static const Color C_SLOT_BG        = {  14,  14,  26, 210 };
static const Color C_SLOT_BORDER    = {  70,  70, 120, 200 };
static const Color C_SLOT_HOVER     = {  35,  45,  75, 230 };
static const Color C_SLOT_DIALOGUE  = {  60, 120, 200, 255 };
static const Color C_SLOT_SOCIAL    = { 100, 200, 120, 255 };

/* ── Helpers ──────────────────────────────────────────────────────────── */

static Rectangle slot_rect(int index) {
    float x = (float)IBUBBLE_MARGIN_X;
    float y = (float)(IBUBBLE_MARGIN_Y + index * (IBUBBLE_ICON_SIZE + IBUBBLE_GAP));
    return (Rectangle){ x, y, (float)IBUBBLE_ICON_SIZE, (float)IBUBBLE_ICON_SIZE };
}

static bool hit_rect(int mx, int my, Rectangle r) {
    return ((float)mx >= r.x && (float)mx < r.x + r.width &&
            (float)my >= r.y && (float)my < r.y + r.height);
}

static void snapshot_layers(InteractionBubbleSlot* slot,
                            const ObjectLayerState* layers, int count,
                            int direction) {
    slot->layer_count = 0;
    slot->direction = direction;
    for (int i = 0; i < count && slot->layer_count < IBUBBLE_MAX_LAYERS; i++) {
        if (layers[i].active && layers[i].item_id[0] != '\0') {
            slot->layers[slot->layer_count] = layers[i];
            slot->layer_count++;
        }
    }
}

static InteractionBubbleSlot* find_slot(const char* entity_id) {
    for (int i = 0; i < s_slot_count; i++) {
        if (strcmp(s_slots[i].entity_id, entity_id) == 0)
            return &s_slots[i];
    }
    return NULL;
}

static InteractionBubbleSlot* upsert_slot(const char* entity_id) {
    InteractionBubbleSlot* slot = find_slot(entity_id);
    if (slot) {
        slot->active = true;
        return slot;
    }
    if (s_slot_count >= IBUBBLE_MAX_SLOTS) return NULL;

    slot = &s_slots[s_slot_count];
    memset(slot, 0, sizeof(InteractionBubbleSlot));
    strncpy(slot->entity_id, entity_id, sizeof(slot->entity_id) - 1);
    slot->active = true;
    slot->appeared_at = GetTime();
    s_slot_count++;
    return slot;
}

static void scan_entity(const char* entity_id, const EntityState* base,
                        bool is_player, const char* behavior) {
    if (!base || !entity_id || entity_id[0] == '\0') return;
    if (base->respawn_in > 0.0f) return;

    uint32_t flags = 0;
    char dlg_item[128] = {0};

    for (int i = 0; i < base->object_layer_count; i++) {
        const ObjectLayerState* ol = &base->object_layers[i];
        if (!ol->active || ol->item_id[0] == '\0') continue;

        dialogue_data_request(ol->item_id);

        if (dialogue_data_available(ol->item_id)) {
            flags |= INTERACT_DIALOGUE;
            if (dlg_item[0] == '\0') {
                strncpy(dlg_item, ol->item_id, sizeof(dlg_item) - 1);
            }
        }
    }

    if (behavior) {
        if (strcmp(behavior, "skill") == 0) return;
        if (strcmp(behavior, "coin")  == 0) return;
    }

    /* Every interactable entity gets INTERACT_SOCIAL so the chat
     * section is available.  In the future the server's bot module
     * will relay chat to AI-driven NPCs the same way it relays to
     * players — the client doesn't need to distinguish. */
    flags |= INTERACT_SOCIAL;

    if (flags == 0) return;

    InteractionBubbleSlot* slot = upsert_slot(entity_id);
    if (!slot) return;

    slot->interact_flags = flags;
    strncpy(slot->dialogue_item_id, dlg_item, sizeof(slot->dialogue_item_id) - 1);

    snapshot_layers(slot, base->object_layers, base->object_layer_count,
                    (int)base->direction);

    if (dlg_item[0] != '\0') {
        const DialogueDataSet* d = dialogue_data_get(dlg_item);
        if (d && d->line_count > 0 && d->lines[0].speaker[0] != '\0') {
            strncpy(slot->display_name, d->lines[0].speaker,
                    sizeof(slot->display_name) - 1);
        } else {
            strncpy(slot->display_name, entity_id, sizeof(slot->display_name) - 1);
        }
    } else {
        strncpy(slot->display_name, entity_id, 8);
        slot->display_name[8] = '\0';
    }
}

/* ── Public API ──────────────────────────────────────────────────────── */

void interaction_bubble_init(void) {
    s_slot_count = 0;
    memset(s_slots, 0, sizeof(s_slots));
}

void interaction_bubble_update(void) {
    for (int i = 0; i < s_slot_count; i++)
        s_slots[i].active = false;

    for (int i = 0; i < g_game_state.other_player_count; i++) {
        scan_entity(
            g_game_state.other_players[i].base.id,
            &g_game_state.other_players[i].base,
            true, NULL
        );
    }

    for (int i = 0; i < g_game_state.bot_count; i++) {
        const BotState* bot = &g_game_state.bots[i];
        if (bot->caster_id[0] != '\0') continue;
        scan_entity(bot->base.id, &bot->base, false, bot->behavior);
    }

    double now = GetTime();
    int write = 0;
    for (int read = 0; read < s_slot_count; read++) {
        bool keep = s_slots[read].active ||
                    (now - s_slots[read].appeared_at < IBUBBLE_MIN_DISPLAY_SEC);
        if (keep) {
            if (write != read)
                s_slots[write] = s_slots[read];
            write++;
        }
    }
    s_slot_count = write;
}

void interaction_bubble_draw(void) {
    if (s_slot_count <= 0) return;

    int mx = GetMouseX();
    int my = GetMouseY();
    ObjectLayersManager* mgr = game_render_get_obj_layers_mgr();

    for (int i = 0; i < s_slot_count; i++) {
        InteractionBubbleSlot* slot = &s_slots[i];
        Rectangle r = slot_rect(i);
        bool hovered = hit_rect(mx, my, r);

        DrawRectangleRounded(r, 0.15f, 4, hovered ? C_SLOT_HOVER : C_SLOT_BG);
        DrawRectangleRoundedLinesEx(r, 0.15f, 4, 1.5f, C_SLOT_BORDER);

        if (mgr && slot->layer_count > 0) {
            for (int j = 0; j < slot->layer_count; j++) {
                ol_as_ico_draw(mgr, slot->layers[j].item_id,
                               (int)r.x + 3, (int)r.y + 3,
                               IBUBBLE_ICON_SIZE - 6,
                               "down_idle", 0, WHITE);
            }
        } else {
            int cx = (int)(r.x + r.width * 0.5f);
            int cy = (int)(r.y + r.height * 0.5f);
            DrawCircle(cx, cy, r.width * 0.3f, (Color){100, 100, 120, 180});
        }

        float dot_r = 4.0f;
        float dx = r.x + r.width - dot_r - 4.0f;
        float dy = r.y + r.height - dot_r - 4.0f;

        if (slot->interact_flags & INTERACT_DIALOGUE) {
            DrawCircle((int)dx, (int)dy, dot_r, C_SLOT_DIALOGUE);
            dy -= dot_r * 2.5f;
        }
        if (slot->interact_flags & INTERACT_SOCIAL) {
            DrawCircle((int)dx, (int)dy, dot_r, C_SLOT_SOCIAL);
        }
    }
}

bool interaction_bubble_handle_click(int mx, int my, bool clicked) {
    if (s_slot_count <= 0 || !clicked) return false;

    for (int i = 0; i < s_slot_count; i++) {
        Rectangle r = slot_rect(i);
        if (hit_rect(mx, my, r)) {
            InteractionBubbleSlot* slot = &s_slots[i];
            printf("[INTERACTION_BUBBLE] Slot %d clicked: entity=%s flags=0x%x\n",
                   i, slot->entity_id, slot->interact_flags);

            /* Open the JS social panel — NO freeze, player stays active
             * in real-time PVP/PVE.  Only NPC dialogue freezes. */
            js_social_overlay_open(slot->entity_id,
                                   slot->display_name,
                                   slot->dialogue_item_id,
                                   slot->interact_flags);
            return true;
        }
    }
    return false;
}

int interaction_bubble_slot_count(void) {
    return s_slot_count;
}
