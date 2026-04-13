/**
 * @file interaction_bubble.c
 * @brief Entity-specific interaction bubbles — full OL-stack icons in a
 *        screen-space column on the left side.
 *
 * Each bubble represents one interactable entity in the player's AOI.
 * The icon is the entity's full active ObjectLayer stack rendered at
 * icon size, so players recognise entities at a glance.
 *
 * Tapping a bubble opens the JS interact overlay (via interact_bridge.h).
 */

#include "interaction_bubble.h"
#include "interact_bridge.h"
#include "dialogue_data.h"
#include "ol_stack_ico.h"
#include "object_layers_management.h"
#include "game_state.h"
#include "entity_render.h"
#include "game_render.h"
#include "client.h"
#include "ui_icon.h"
#include "notify_badge.h"
#include <raylib.h>
#include <string.h>
#include <stdio.h>

/* ── Module state ─────────────────────────────────────────────────────── */

static InteractionBubbleSlot s_slots[IBUBBLE_MAX_SLOTS];
static int                   s_slot_count = 0;
static int                   s_border_color_dbg = 0;

/* ── Colours ──────────────────────────────────────────────────────────── */

static const Color C_SLOT_BG        = {  14,  14,  26, 210 };
static const Color C_SLOT_HOVER     = {  35,  45,  75, 230 };

/**
 * @brief Return the border colour for a bubble slot.
 * Self-player uses the SELF_BORDER palette colour; everyone else
 * uses the per-status border colour from the StatusIconConfig table
 * (server-driven, received in init_data).
 */
static Color status_border_color(const InteractionBubbleSlot* slot, bool is_self) {
    if (is_self) return game_state_get_color_by_key("SELF_BORDER");
    Color c = game_state_get_status_border_color(slot->status_icon);
    if (s_border_color_dbg < 12) {
        printf("[BORDER] entity=%s status_icon=%d border=(%d,%d,%d,%d) count=%d\n",
               slot->entity_id, slot->status_icon,
               c.r, c.g, c.b, c.a,
               g_game_state.status_icon_count);
        s_border_color_dbg++;
    }
    return c;
}

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

    if (behavior) {
        if (strcmp(behavior, "skill") == 0) return;
        if (strcmp(behavior, "coin")  == 0) return;
    }

    bool is_dead = (base->respawn_in > 0.0f);

    /* Determine interaction capabilities from the ALIVE layers.
     * When dead the server sends ghost OLs — we still want to know
     * what dialogue the entity had, so we use the cached alive_layers
     * (populated on a prior alive frame) or fall back to current layers
     * for the very first scan (entity spawned alive). */
    InteractionBubbleSlot* existing = find_slot(entity_id);

    uint32_t flags = 0;
    char dlg_item[128] = {0};

    if (!is_dead) {
        /* Entity is alive — scan its current active OLs normally. */
        for (int i = 0; i < base->object_layer_count; i++) {
            const ObjectLayerState* ol = &base->object_layers[i];
            if (!ol->active || ol->item_id[0] == '\0') continue;

            dialogue_data_request(ol->item_id);

            if (dialogue_data_available(ol->item_id)) {
                flags |= INTERACT_DIALOGUE;
                if (dlg_item[0] == '\0')
                    strncpy(dlg_item, ol->item_id, sizeof(dlg_item) - 1);
            }
        }
    } else if (existing && existing->alive_layer_count > 0) {
        /* Entity is dead — reuse cached alive layers for dialogue check. */
        for (int i = 0; i < existing->alive_layer_count; i++) {
            const ObjectLayerState* ol = &existing->alive_layers[i];
            if (!ol->active || ol->item_id[0] == '\0') continue;

            dialogue_data_request(ol->item_id);

            if (dialogue_data_available(ol->item_id)) {
                flags |= INTERACT_DIALOGUE;
                if (dlg_item[0] == '\0')
                    strncpy(dlg_item, ol->item_id, sizeof(dlg_item) - 1);
            }
        }
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
    slot->status_icon = base->status_icon;
    slot->is_player = is_player;

    /* Always snapshot current layers (dead or alive) into layers[]. */
    snapshot_layers(slot, base->object_layers, base->object_layer_count,
                    (int)base->direction);

    /* Cache alive layers: only update when entity is alive.
     * When dead, alive_layers[] retains the last alive snapshot. */
    if (!is_dead) {
        slot->alive_layer_count = 0;
        for (int i = 0; i < base->object_layer_count && slot->alive_layer_count < IBUBBLE_MAX_LAYERS; i++) {
            if (base->object_layers[i].active && base->object_layers[i].item_id[0] != '\0') {
                slot->alive_layers[slot->alive_layer_count] = base->object_layers[i];
                slot->alive_layer_count++;
            }
        }
    }

    /* Dialogue item — prefer alive data, fall back to existing cache. */
    if (dlg_item[0] != '\0') {
        strncpy(slot->dialogue_item_id, dlg_item, sizeof(slot->dialogue_item_id) - 1);
    } else if (!existing || existing->dialogue_item_id[0] == '\0') {
        slot->dialogue_item_id[0] = '\0';
    }

    /* Display name: for bots with dialogue, use speaker name (= skin label).
     * For players and everything else, use the full entity ID (= websocket ID). */
    if (!is_player && dlg_item[0] != '\0') {
        const DialogueDataSet* d = dialogue_data_get(dlg_item);
        if (d && d->line_count > 0 && d->lines[0].speaker[0] != '\0') {
            strncpy(slot->display_name, d->lines[0].speaker,
                    sizeof(slot->display_name) - 1);
        } else {
            strncpy(slot->display_name, entity_id, sizeof(slot->display_name) - 1);
        }
    } else {
        strncpy(slot->display_name, entity_id, sizeof(slot->display_name) - 1);
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

    /* Self-player is always scanned first → occupies slot 0. */
    if (g_game_state.player_id[0] != '\0') {
        scan_entity(g_game_state.player_id,
                    &g_game_state.player.base, true, NULL);
    }

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
        bool is_self = (strcmp(slot->entity_id, g_game_state.player_id) == 0);

        Color border = status_border_color(slot, is_self);
        float thick = 3.0f;

        /* Draw border as a slightly larger filled rounded rect behind the slot.
         * DrawRectangleRoundedLinesEx uses GL line primitives which are
         * unreliable / invisible in many WebGL contexts, so we simulate
         * the border with two filled rounded rects instead. */
        Rectangle outer = { r.x - thick, r.y - thick,
                            r.width + 2*thick, r.height + 2*thick };
        DrawRectangleRounded(outer, 0.15f, 4, border);
        DrawRectangleRounded(r, 0.15f, 4, hovered ? C_SLOT_HOVER : C_SLOT_BG);

        /* Always render the alive OL stack so the entity is recognisable
         * even when dead (ghost state is indicated by the status icon). */
        int icon_lc = slot->alive_layer_count > 0 ? slot->alive_layer_count : slot->layer_count;
        const ObjectLayerState* icon_layers = slot->alive_layer_count > 0 ? slot->alive_layers : slot->layers;

        if (mgr && icon_lc > 0) {
            ol_stack_ico_draw(mgr, icon_layers, icon_lc,
                              (int)r.x + 3, (int)r.y + 3,
                              IBUBBLE_ICON_SIZE - 6,
                              "down_idle", 0, WHITE);
        } else {
            int cx = (int)(r.x + r.width * 0.5f);
            int cy = (int)(r.y + r.height * 0.5f);
            DrawCircle(cx, cy, r.width * 0.3f, (Color){100, 100, 120, 180});
        }

        /* Status icon — small icon in bottom-right corner of the bubble */
        if (slot->status_icon != 0) {
            const char* icon_id = NULL;
            for (int si = 0; si < g_game_state.status_icon_count; si++) {
                if (g_game_state.status_icons[si].id == slot->status_icon) {
                    icon_id = g_game_state.status_icons[si].icon_id;
                    break;
                }
            }
            if (icon_id) {
                int ico_sz = 16;
                float ix = r.x + r.width - ico_sz * 0.5f - 2.0f;
                float iy = r.y + r.height - ico_sz * 0.5f - 2.0f;
                /* Phase from entity_id hash for desync */
                unsigned int h = 0;
                for (const char* c = slot->entity_id; *c; c++)
                    h = h * 31 + (unsigned char)*c;
                float phase = (float)(h % 1000) * 0.001f * 6.2832f;
                ui_icon_draw(icon_id, ix, iy, ico_sz, false, phase);
            }
        }

        /* Notification badge — red circle with unread count, top-right */
        int badge_n = js_notify_badge_count(slot->entity_id);
        if (badge_n > 0) {
            float br = 8.0f;
            float bx = r.x + r.width - br;
            float by = r.y + br;
            DrawCircle((int)bx, (int)by, br, (Color){200, 50, 50, 230});
            char badge_txt[8];
            snprintf(badge_txt, sizeof(badge_txt), "%d", badge_n > 99 ? 99 : badge_n);
            int bfs = 10;
            int btw = MeasureText(badge_txt, bfs);
            DrawText(badge_txt, (int)(bx - btw * 0.5f), (int)(by - bfs * 0.5f),
                     bfs, (Color){255, 255, 255, 240});
        }
    }
}

bool interaction_bubble_handle_click(int mx, int my, bool clicked) {
    if (s_slot_count <= 0 || !clicked) return false;

    ObjectLayersManager* mgr = game_render_get_obj_layers_mgr();

    for (int i = 0; i < s_slot_count; i++) {
        Rectangle r = slot_rect(i);
        if (hit_rect(mx, my, r)) {
            InteractionBubbleSlot* slot = &s_slots[i];
            printf("[INTERACTION_BUBBLE] Slot %d clicked: entity=%s flags=0x%x\n",
                   i, slot->entity_id, slot->interact_flags);

            /* Open the JS interact panel — NO freeze, player stays active
             * in real-time PVP/PVE.  Only NPC dialogue freezes. */
            bool is_self = (strcmp(slot->entity_id, g_game_state.player_id) == 0);
            Color bc = status_border_color(slot, is_self);
            js_interact_overlay_open(slot->entity_id,
                                     slot->display_name,
                                     slot->dialogue_item_id,
                                     slot->interact_flags,
                                     slot->is_player ? 1 : 0,
                                     is_self ? 1 : 0,
                                     (int)bc.r, (int)bc.g,
                                     (int)bc.b, (int)bc.a);

            /* Pass the entity's active OL stack to the JS overlay for
             * preview rendering and per-item dialog buttons.  Each entry
             * includes itemId, type (for static asset URL construction),
             * and a hasDialogue flag. */
            {
                int icon_lc = slot->alive_layer_count > 0
                    ? slot->alive_layer_count : slot->layer_count;
                const ObjectLayerState* icon_layers = slot->alive_layer_count > 0
                    ? slot->alive_layers : slot->layers;

                char json[4096];
                int off = 0;
                json[off++] = '[';

                for (int j = 0; j < icon_lc; j++) {
                    if (!icon_layers[j].active || icon_layers[j].item_id[0] == '\0')
                        continue;

                    const char* item_type = "";
                    if (mgr) {
                        ObjectLayer* ol_data = get_or_fetch_object_layer(
                            mgr, icon_layers[j].item_id);
                        if (ol_data && ol_data->data.item.type[0] != '\0')
                            item_type = ol_data->data.item.type;
                    }

                    bool has_dlg = dialogue_data_available(icon_layers[j].item_id);

                    int wrote = snprintf(json + off, sizeof(json) - off,
                        "%s{\"itemId\":\"%s\",\"type\":\"%s\",\"hasDialogue\":%s}",
                        off > 1 ? "," : "",
                        icon_layers[j].item_id,
                        item_type,
                        has_dlg ? "true" : "false");
                    if (wrote > 0 && off + wrote < (int)sizeof(json) - 2)
                        off += wrote;
                }

                json[off++] = ']';
                json[off] = '\0';
                js_interact_overlay_set_ol_stack(json);
            }
            return true;
        }
    }
    return false;
}

int interaction_bubble_slot_count(void) {
    return s_slot_count;
}

/* ── Dead-equip: optimistically update self-player alive_layers ──────── */
void interaction_bubble_dead_equip(const char* item_id, bool active) {
    if (s_slot_count <= 0) return;
    /* Self-player is always slot 0. */
    InteractionBubbleSlot* slot = &s_slots[0];
    if (strcmp(slot->entity_id, g_game_state.player_id) != 0) return;

    if (active) {
        /* Activate: mark matching item active, apply one-per-type rule. */
        int target = -1;
        for (int i = 0; i < slot->alive_layer_count; i++) {
            if (strcmp(slot->alive_layers[i].item_id, item_id) == 0) {
                slot->alive_layers[i].active = true;
                target = i;
                break;
            }
        }
        /* Item wasn't in alive_layers (was inactive before death).
         * Pull it from the full inventory and append to alive_layers. */
        if (target < 0 && slot->alive_layer_count < IBUBBLE_MAX_LAYERS) {
            for (int i = 0; i < g_game_state.full_inventory_count; i++) {
                if (strcmp(g_game_state.full_inventory[i].item_id, item_id) == 0) {
                    int idx = slot->alive_layer_count++;
                    slot->alive_layers[idx] = g_game_state.full_inventory[i];
                    slot->alive_layers[idx].active = true;
                    target = idx;
                    break;
                }
            }
        }
        /* One-per-type deactivation (mirrors server logic). */
        if (target >= 0 && g_game_state.equipment_rules.one_per_type) {
            ObjectLayersManager* mgr = game_render_get_obj_layers_mgr();
            const char* req_type = "";
            if (mgr) {
                ObjectLayer* ol = get_or_fetch_object_layer(mgr, item_id);
                if (ol && ol->data.item.type[0] != '\0')
                    req_type = ol->data.item.type;
            }
            if (req_type[0] != '\0') {
                for (int j = 0; j < slot->alive_layer_count; j++) {
                    if (j == target || !slot->alive_layers[j].active) continue;
                    if (mgr) {
                        ObjectLayer* other = get_or_fetch_object_layer(
                            mgr, slot->alive_layers[j].item_id);
                        if (other && strcmp(other->data.item.type, req_type) == 0)
                            slot->alive_layers[j].active = false;
                    }
                }
            }
        }
    } else {
        /* Deactivate. */
        for (int i = 0; i < slot->alive_layer_count; i++) {
            if (strcmp(slot->alive_layers[i].item_id, item_id) == 0) {
                slot->alive_layers[i].active = false;
                break;
            }
        }
    }
    printf("[INTERACTION_BUBBLE] Dead-equip: item=%s active=%d\n", item_id, active);
}
