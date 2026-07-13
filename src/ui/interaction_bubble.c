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
#include "text.h"

#include "network/game_client.h"
#include "domain/presentation_runtime.h"
#include "domain/viewport.h"
#include "dialogue_data.h"
#include "entity_render.h"
#include "game_state.h"
#include "world_types.h"
#include "js/interact_bridge.h"
#include "modal_interact.h"
#include "notification.h"
#include "notify_store.h"
#include "layer_z_order.h"
#include "inventory_bar.h"
#include "ui_toggle.h"
#include "ui_scroll.h"
#include "nameplate.h"
#include "object_layers_management.h"
#include "ol_stack_ico.h"
#include "ui_icon.h"
#include "util/log.h"

#include <assert.h>
#include <raylib.h>
#include <stdio.h>
#include <string.h>

/* ── Module state ─────────────────────────────────────────────────────── */

static InteractionBubbleSlot s_slots[IBUBBLE_MAX_SLOTS];
static int                   s_slot_count = 0;
static int                   s_border_color_dbg = 0;

/* Collapsible column: a left-edge toggle slides the bubbles in/out. Slots
 * keep updating while collapsed (entities stay tracked). */
/* Small square toggle matching the Quest Journal chevron size. */
#define IBUBBLE_TOGGLE_SZ  32
#define IBUBBLE_TOGGLE_PAD  6
#define IBUBBLE_SCROLL_GAP   6.0f
static UIToggle s_col_toggle;
static bool     s_col_init = false;
/* Horizontal slide: 0 when expanded, −column_width when fully collapsed. */
static float    s_col_offset = 0.0f;
static UIScroll s_col_scroll;

static bool open_slot_at(int mx, int my);

static float column_width(void) {
    return (float)(IBUBBLE_MARGIN_X + IBUBBLE_ICON_SIZE + IBUBBLE_GAP);
}

/* Minimum label/chat overhang assumed right of the icon lane; the draw pass
 * grows it from the measured content so the slide always travels far enough
 * for the WIDEST nameplate / chat bubble to clear the left edge — nothing is
 * left to pop off when the draw gate closes. */
#define IBUBBLE_SLIDE_REACH_MIN 260.0f

/* Fixed slide duration, both directions. The offset derives from a
 * normalized progress with a smoothstep ease, so hide departs exactly as
 * gently as show arrives no matter how far the measured reach extends the
 * travel (a time-constant ease toward the target would instead spend its
 * fastest stretch on the visible part of the hide). */
#define IBUBBLE_SLIDE_DURATION 0.35f

static float s_col_reach   = IBUBBLE_SLIDE_REACH_MIN;
static float s_col_slide_t = 1.0f; /* 0 = fully hidden, 1 = fully expanded */

static float column_slide_width(void) {
    return column_width() + s_col_reach;
}

/* Fully off screen: collapsed and the slide has finished. */
static bool column_hidden(void) {
    return !s_col_toggle.expanded && 0.0f >= s_col_slide_t;
}

/* Fixed screen-space anchor: top-left corner with small padding.
 * The toggle never moves — it is always at the same screen position. */
static Rectangle toggle_anchor(void) {
    return (Rectangle){ (float)IBUBBLE_TOGGLE_PAD, (float)IBUBBLE_TOGGLE_PAD,
                        (float)IBUBBLE_TOGGLE_SZ,  (float)IBUBBLE_TOGGLE_SZ };
}

static Rectangle column_scroll_view(void) {
    Rectangle inventory_toggle = inventory_bar_toggle_bounds();
    float top = s_col_toggle.anchor.y + s_col_toggle.anchor.height + IBUBBLE_SCROLL_GAP;
    float bottom = inventory_toggle.y - IBUBBLE_SCROLL_GAP;
    if (bottom < top) bottom = top;
    return (Rectangle){ 0.0f, top, (float)GetScreenWidth(), bottom - top };
}

static Rectangle column_input_bounds(Rectangle view) {
    float right = (float)IBUBBLE_MARGIN_X + s_col_offset +
                  (float)IBUBBLE_ICON_SIZE + (float)IBUBBLE_GAP;
    if (right < 0.0f) right = 0.0f;
    if (right > view.width) right = view.width;
    return (Rectangle){ 0.0f, view.y, right, view.height };
}

static Rectangle column_scrollbar_bounds(Rectangle view) {
    return (Rectangle){ 0.0f, view.y, (float)IBUBBLE_MARGIN_X, view.height };
}

static float column_content_height(void) {
    return (float)IBUBBLE_MARGIN_Y +
           (float)s_slot_count * (float)(IBUBBLE_ICON_SIZE + IBUBBLE_GAP);
}

static void column_ensure_toggle(void) {
    if (s_col_init) return;
    bool expanded = !viewport_is_mobile();
    s_col_slide_t = expanded ? 1.0f : 0.0f;
    s_col_offset  = expanded ? 0.0f : -column_slide_width();
    /* Chevron points LEFT when expanded ("tap to collapse"); flips to RIGHT
     * when collapsed via resolve_chevron so it means "tap to expand". */
    ui_toggle_init(&s_col_toggle, toggle_anchor(), expanded, UI_TOGGLE_CHEVRON_LEFT);
    ui_scroll_reset(&s_col_scroll);
    s_col_init = true;
}

/* ── Colours ──────────────────────────────────────────────────────────── */

static const Color C_SLOT_BG        = {  14,  14,  26, 210 };
static const Color C_SLOT_HOVER     = {  35,  45,  75, 230 };

/**
 * @brief Return the border colour for a bubble slot.
 * Self-player uses the SELF_BORDER palette key (client-owned); everyone
 * else uses the per-status border colour resolved against the client's
 * presentation table. Neither path consults the server.
 */
static Color status_border_color(const InteractionBubbleSlot* slot, bool is_self) {
    if (is_self) return presentation_runtime_palette("SELF_BORDER");
    Color c = presentation_runtime_status_border(slot->status_icon);
    if (s_border_color_dbg < 12) {
        LOG_INFO("[BORDER] entity=%s status_icon=%d border=(%d,%d,%d,%d)\n",
               slot->entity_id, slot->status_icon, c.r, c.g, c.b, c.a);
        s_border_color_dbg++;
    }
    return c;
}

/* ── Helpers ──────────────────────────────────────────────────────────── */

static Rectangle slot_rect(int index, Rectangle view) {
    float x = (float)IBUBBLE_MARGIN_X + s_col_offset;
    float y = view.y + (float)IBUBBLE_MARGIN_Y +
              (float)index * (float)(IBUBBLE_ICON_SIZE + IBUBBLE_GAP) -
              ui_scroll_offset(&s_col_scroll);
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

/* Item id of the entity's active skin, or NULL. Talk dialogue is keyed to
 * the skin only — the displayed dialogue is the one associated with it. */
static const char* active_skin_item_id(const ObjectLayerState* layers, int count) {
    for (int i = 0; i < count; i++) {
        if (!layers[i].active || '\0' == layers[i].item_id[0]) continue;
        ObjectLayer* ol = lookup_cached_layer(layers[i].item_id);
        if (ol && 0 == strcmp(ol->data.item.type, "skin")) return layers[i].item_id;
    }
    return NULL;
}

static void scan_entity(const char* entity_id, const EntityState* base,
                        bool is_player, const char* behavior,
                        uint8_t interaction_flags) {
    if (!base || !entity_id || entity_id[0] == '\0') return;

    if (behavior) {
        if (strcmp(behavior, "skill") == 0) return;
        if (strcmp(behavior, "coin")  == 0) return;
        if (strcmp(behavior, "drop")  == 0) return; /* loot tokens are not interactable */
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

    /* Talk is available only when the entity's ACTIVE SKIN has dialogue —
     * the shown dialogue is the skin's. Alive entities use their current
     * OLs; dead ones reuse the cached alive snapshot. */
    const ObjectLayerState* scan_layers = NULL;
    int scan_count = 0;
    if (!is_dead) {
        scan_layers = base->object_layers;
        scan_count  = base->object_layer_count;
    } else if (existing && existing->alive_layer_count > 0) {
        scan_layers = existing->alive_layers;
        scan_count  = existing->alive_layer_count;
    }

    if (scan_layers) {
        const char* skin = active_skin_item_id(scan_layers, scan_count);
        if (skin) {
            dialogue_data_request(skin);
            if (dialogue_data_available(skin)) {
                flags |= INTERACT_DIALOGUE;
                strncpy(dlg_item, skin, sizeof(dlg_item) - 1);
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
    slot->interaction_flags = interaction_flags;
    slot->is_player = is_player;

    /* Resolve the solid-colour fallback from the client-owned presentation
     * table by entity_type. The server does not ship colour data. */
    const char* etype = is_player ? "player" : "bot";
    slot->fallback_color = presentation_runtime_entity_fallback_color(etype);

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

    /* Display name: resolved centrally by the nameplate module.
     * Players → "AnonPlayer<first 8 chars of ws id>".
     * Bots    → skin/body item_id (with manager lookup). */
    {
        ObjectLayersManager* np_mgr = obj_layers_mgr_get();
        const ObjectLayerState* np_layers = slot->alive_layer_count > 0
            ? slot->alive_layers : base->object_layers;
        int np_lc = slot->alive_layer_count > 0
            ? slot->alive_layer_count : base->object_layer_count;
        nameplate_resolve(entity_id, is_player,
                          np_layers, np_lc, np_mgr,
                          slot->display_name,
                          (int)sizeof(slot->display_name));
    }
}

/* ── Public API ──────────────────────────────────────────────────────── */

void interaction_bubble_init(void) {
    s_slot_count = 0;
    memset(s_slots, 0, sizeof(s_slots));
    s_col_init = false;
    s_col_reach = IBUBBLE_SLIDE_REACH_MIN;
    ui_scroll_reset(&s_col_scroll);
}

void interaction_bubble_update(void) {
    column_ensure_toggle();
    /* Re-anchor not needed — the toggle is fixed in screen space. */
    ui_toggle_update(&s_col_toggle, GetFrameTime());
    /* Fixed-duration slide progress, independent of the chevron's anim_t. */
    float dir = s_col_toggle.expanded ? 1.0f : -1.0f;
    s_col_slide_t += dir * GetFrameTime() / IBUBBLE_SLIDE_DURATION;
    if (s_col_slide_t < 0.0f) s_col_slide_t = 0.0f;
    if (s_col_slide_t > 1.0f) s_col_slide_t = 1.0f;
    float ease = s_col_slide_t * s_col_slide_t * (3.0f - 2.0f * s_col_slide_t);
    s_col_offset = -column_slide_width() * (1.0f - ease);

    for (int i = 0; i < s_slot_count; i++)
        s_slots[i].active = false;

    /* Self-player is always scanned first → occupies slot 0. */
    if (g_game_state.player_id[0] != '\0') {
        scan_entity(g_game_state.player_id,
                    &g_game_state.player.base, true, NULL, 0);
    }

    for (int i = 0; i < g_game_state.other_player_count; i++) {
        scan_entity(
            g_game_state.other_players[i].base.id,
            &g_game_state.other_players[i].base,
            true, NULL, 0
        );
    }

    for (int i = 0; i < g_game_state.bot_count; i++) {
        const BotState* bot = &g_game_state.bots[i];
        if (bot->caster_id[0] != '\0') continue;
        scan_entity(bot->base.id, &bot->base, false, bot->behavior, bot->interaction_flags);
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

    /* Keep the scroll geometry (the draw scissor's view) current whenever the
     * column is visible — including the slide-out — so it is never clipped by
     * a stale rect. Clicks are consumed only while expanded. */
    if (!column_hidden()) {
        Rectangle view = column_scroll_view();
        ui_scroll_set_input_bounds(&s_col_scroll, column_input_bounds(view));
        ui_scroll_set_scrollbar_bounds(&s_col_scroll, column_scrollbar_bounds(view));
        ui_scroll_update(&s_col_scroll, view, column_content_height(), GetFrameTime());
        int click_x, click_y;
        if (ui_scroll_take_click(&s_col_scroll, &click_x, &click_y) &&
            s_col_toggle.expanded) {
            open_slot_at(click_x, click_y);
        }
    }
}

/* Aggregate unread badge on the collapsed toggle — every register, every
 * target. Per-target chat previews render only while the column is open,
 * so this counter is the sole unread indicator while it is collapsed. */
static void draw_collapsed_total_badge(void) {
    if (s_col_toggle.expanded) return;
    int total = notification_total();
    if (0 >= total) return;

    Rectangle a  = s_col_toggle.anchor;
    int   fs = 11;
    float br = 9.0f;
    float cx = a.x + a.width;
    float cy = a.y + 4.0f;
    DrawCircle((int)cx, (int)cy, br, (Color){ 210, 60, 60, 245 });
    char buf[8];
    snprintf(buf, sizeof(buf), "%d", total > 99 ? 99 : total);
    int tw = MeasureText(buf, fs);
    DrawText(buf, (int)(cx - tw * 0.5f), (int)(cy - fs * 0.5f), fs,
             (Color){ 255, 255, 255, 245 });
}

void interaction_bubble_draw(void) {
    column_ensure_toggle();
    /* Keep drawing until the slide offset settles off screen so collapsing
     * plays the same slide the expand does, instead of vanishing on the tap. */
    if (column_hidden() || s_slot_count <= 0) {
        ui_toggle_draw(&s_col_toggle);
        draw_collapsed_total_badge();
        return;
    }

    int mx = GetMouseX();
    int my = GetMouseY();
    ObjectLayersManager* mgr = obj_layers_mgr_get();
    Rectangle view = column_scroll_view();

    ui_scroll_begin(&s_col_scroll);
    for (int i = 0; i < s_slot_count; i++) {
        InteractionBubbleSlot* slot = &s_slots[i];
        Rectangle r = slot_rect(i, view);
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
            DrawCircle(cx, cy, r.width * 0.3f, slot->fallback_color);
        }

        /* Nameplate label — top-left, just to the right of the icon. Base size
         * is larger than the surrounding UI so the name reads clearly after the
         * global font multiplier (text.h shim) scales it. */
        int np_fs = 13;
        int np_x  = (int)(r.x + r.width + 4);
        int np_y  = (int)(r.y + 2);
        if (slot->display_name[0] != '\0') {
            DrawText(slot->display_name, np_x + 1, np_y + 1, np_fs,
                     (Color){0, 0, 0, 180});
            DrawText(slot->display_name, np_x, np_y, np_fs,
                     (Color){220, 220, 230, 240});
            /* Grow the slide reach to this label's overhang past the icon. */
            float reach = r.width + 4.0f +
                          (float)MeasureText(slot->display_name, np_fs) + 8.0f;
            if (reach > s_col_reach) s_col_reach = reach;
        }

        /* Status icons — the presence lifecycle icon plus one overlay per set
         * interaction-capability bit (action, quest). Drawn as a small row in
         * the bottom-right; the server ships the presence u8 + capability
         * bitmask, the client resolves each stem from presentation_runtime. */
        uint8_t status_ids[3];
        int status_n = 0;
        if (slot->status_icon != 0) status_ids[status_n++] = slot->status_icon;
        if (slot->interaction_flags & INTERACTION_FLAG_ACTION)
            status_ids[status_n++] = STATUS_ICON_ACTION_PROVIDER;
        if (slot->interaction_flags & INTERACTION_FLAG_QUEST)
            status_ids[status_n++] = STATUS_ICON_QUEST_PROVIDER;
        if (status_n > 0) {
            int ico_sz = 16;
            unsigned int h = 0;
            for (const char* c = slot->entity_id; *c; c++)
                h = h * 31 + (unsigned char)*c;
            float phase = (float)(h % 1000) * 0.001f * 6.2832f;
            float iy = r.y + r.height - ico_sz * 0.5f - 2.0f;
            for (int k = 0; k < status_n; k++) {
                const char* icon_id = presentation_runtime_status_icon(status_ids[k]);
                if (!icon_id || icon_id[0] == '\0') continue;
                float ix = r.x + r.width - ico_sz * 0.5f - 2.0f - (float)k * (ico_sz + 2.0f);
                ui_icon_draw(icon_id, ix, iy, ico_sz, false, phase);
            }
        }

        /* Last chat message — informational bubble below the name, prefixed
         * with a red badge of the target's total notification count. Shown
         * only while there are unread notifications; reading the chat (Chat
         * button) clears the count and hides this. Never intercepts taps. */
        const NotifyEntry* ne = notify_store_get(slot->entity_id);
        int notif = notification_target_total(slot->entity_id);
        if (notif > 0 && ne && ne->count > 0 && ne->messages[ne->count - 1].text[0] != '\0') {
            const NotifyMessage* last = &ne->messages[ne->count - 1];
            int mfs = 11;
            char buf[48];
            strncpy(buf, last->text, sizeof(buf) - 1);
            buf[sizeof(buf) - 1] = '\0';
            if (strlen(last->text) > sizeof(buf) - 1) {
                buf[sizeof(buf) - 2] = '.';
                buf[sizeof(buf) - 3] = '.';
            }
            /* Fade in on arrival for a subtle "new message" pop. */
            double age  = GetTime() * 1000.0 - last->ts_ms;
            float  fade = age < 220.0 ? (float)(age / 220.0) : 1.0f;

            int count   = notif;
            int padx    = 6, pady = 4;
            int br      = count > 0 ? (int)(mfs * 0.72f) : 0;
            int badge_w = count > 0 ? br * 2 + 5 : 0;
            int tw      = MeasureText(buf, mfs);

            float bubx = (float)np_x - padx;
            float buby = (float)(np_y + np_fs + 5);
            float bubw = (float)(padx * 2 + badge_w + tw);
            float bubh = (float)(mfs + pady * 2);
            Rectangle bub = { bubx, buby, bubw, bubh };
            /* Grow the slide reach to this chat bubble's overhang. */
            float reach = (bubx + bubw + 8.0f) - r.x;
            if (reach > s_col_reach) s_col_reach = reach;
            DrawRectangleRounded(bub, 0.5f, 6, (Color){ 18, 22, 38, (unsigned char)(225 * fade) });
            DrawRectangleRoundedLinesEx(bub, 0.5f, 6, 1.0f,
                                        (Color){ border.r, border.g, border.b,
                                                 (unsigned char)(150 * fade) });

            if (count > 0) {
                float bcx = bubx + padx + br;
                float bcy = buby + bubh * 0.5f;
                DrawCircle((int)bcx, (int)bcy, (float)br, (Color){ 210, 60, 60, (unsigned char)(245 * fade) });
                char cbuf[8];
                snprintf(cbuf, sizeof(cbuf), "%d", count > 99 ? 99 : count);
                int cfs = mfs - 1;
                int ctw = MeasureText(cbuf, cfs);
                DrawText(cbuf, (int)(bcx - ctw * 0.5f), (int)(bcy - cfs * 0.5f), cfs,
                         (Color){ 255, 255, 255, (unsigned char)(245 * fade) });
            }

            DrawText(buf, (int)(bubx + padx + badge_w), (int)(buby + pady), mfs,
                     (Color){ 210, 220, 240, (unsigned char)(245 * fade) });
        }
    }
    ui_scroll_end(&s_col_scroll);
    ui_toggle_draw(&s_col_toggle);
    draw_collapsed_total_badge();
}

static bool open_slot_at(int mx, int my) {
    Rectangle view = column_scroll_view();
    for (int i = 0; i < s_slot_count; i++) {
        Rectangle r = slot_rect(i, view);
        if (hit_rect(mx, my, r)) {
            InteractionBubbleSlot* slot = &s_slots[i];
            LOG_INFO("[INTERACTION_BUBBLE] Slot %d clicked: entity=%s flags=0x%x\n",
                   i, slot->entity_id, slot->interact_flags);

            /* Bubble tap opens modal_interact. has_dialogue is true when the
             * active skin has dialogue; the capability bitmask gates the Action
             * and Quest tabs. */
            bool is_self      = (strcmp(slot->entity_id, g_game_state.player_id) == 0);
            bool has_dialogue = (slot->interact_flags & INTERACT_DIALOGUE) != 0 &&
                                slot->dialogue_item_id[0] != '\0';
            Color bc = status_border_color(slot, is_self);
            modal_interact_open(slot->entity_id, slot->display_name,
                                slot->dialogue_item_id, has_dialogue,
                                slot->interaction_flags, bc);
            return true;
        }
    }
    return false;
}

bool interaction_bubble_handle_click(int mx, int my) {
    column_ensure_toggle();
    if (ui_toggle_handle_click(&s_col_toggle, mx, my)) {
        /* Reset the scroll only when opening. Resetting on collapse would
         * zero the scroll's stored view — a 0×0 scissor — and scissor the
         * column away on the next frame instead of letting it slide out. */
        if (s_col_toggle.expanded) ui_scroll_reset(&s_col_scroll);
        return true;
    }

    if (!s_col_toggle.expanded || s_slot_count <= 0) return false;
    Rectangle view = column_scroll_view();
    Rectangle input = column_input_bounds(view);
    if (!hit_rect(mx, my, input)) return false;
    ui_scroll_on_press(&s_col_scroll, mx, my);
    return true;
}

bool interaction_bubble_handle_wheel(float wheel_delta) {
    column_ensure_toggle();
    if (!s_col_toggle.expanded || s_slot_count <= 0) return false;
    Rectangle view = column_scroll_view();
    ui_scroll_set_input_bounds(&s_col_scroll, column_input_bounds(view));
    ui_scroll_set_scrollbar_bounds(&s_col_scroll, column_scrollbar_bounds(view));
    return ui_scroll_on_wheel(&s_col_scroll, view, column_content_height(), wheel_delta);
}

/* Open the JS overlay for one resolved slot on a given tab, pushing its OL
 * stack for preview rendering. NO freeze — the overlay is real-time-safe. */
static void open_js_overlay_for_slot(InteractionBubbleSlot* slot, int initial_tab) {
    bool is_self = (strcmp(slot->entity_id, g_game_state.player_id) == 0);
    Color bc = status_border_color(slot, is_self);
    js_interact_overlay_open(slot->entity_id,
                             slot->display_name,
                             slot->dialogue_item_id,
                             slot->interact_flags,
                             slot->is_player ? 1 : 0,
                             is_self ? 1 : 0,
                             (int)bc.r, (int)bc.g,
                             (int)bc.b, (int)bc.a,
                             initial_tab);

    int icon_lc = slot->alive_layer_count > 0
        ? slot->alive_layer_count : slot->layer_count;
    const ObjectLayerState* icon_layers = slot->alive_layer_count > 0
        ? slot->alive_layers : slot->layers;

    LayerZEntry z_sorted[32];
    int z_count = layer_z_sort(icon_layers, icon_lc, z_sorted, 32, false);

    char json[4096];
    int off = 0;
    json[off++] = '[';

    for (int j = 0; j < z_count; j++) {
        const ObjectLayerState* ls = &icon_layers[z_sorted[j].index];

        const char* item_type = "";
        ObjectLayer* ol_data = lookup_cached_layer(ls->item_id);
        if (ol_data && ol_data->data.item.type[0] != '\0')
            item_type = ol_data->data.item.type;

        bool has_dlg = dialogue_data_available(ls->item_id);

        int wrote = snprintf(json + off, sizeof(json) - off,
            "%s{\"itemId\":\"%s\",\"type\":\"%s\",\"hasDialogue\":%s}",
            off > 1 ? "," : "",
            ls->item_id,
            item_type,
            has_dlg ? "true" : "false");
        if (wrote > 0 && off + wrote < (int)sizeof(json) - 2)
            off += wrote;
    }

    json[off++] = ']';
    json[off] = '\0';
    js_interact_overlay_set_ol_stack(json);
}

void interaction_bubble_open_js_overlay(const char* entity_id, int initial_tab) {
    if (!entity_id || '\0' == entity_id[0]) return;
    for (int i = 0; i < s_slot_count; i++) {
        if (0 == strcmp(s_slots[i].entity_id, entity_id)) {
            open_js_overlay_for_slot(&s_slots[i], initial_tab);
            return;
        }
    }
}

bool interaction_bubble_point_covered(int x, int y) {
    column_ensure_toggle();
    if (hit_rect(x, y, s_col_toggle.anchor)) return true; /* tab always blocks */
    if (!s_col_toggle.expanded) return false;             /* collapsed → free for game */
    if (s_slot_count <= 0) return false;
    return hit_rect(x, y, column_input_bounds(column_scroll_view()));
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
            const char* req_type = "";
            ObjectLayer* ol = lookup_cached_layer(item_id);
            if (ol && ol->data.item.type[0] != '\0')
                req_type = ol->data.item.type;
            if (req_type[0] != '\0') {
                for (int j = 0; j < slot->alive_layer_count; j++) {
                    if (j == target || !slot->alive_layers[j].active) continue;
                    ObjectLayer* other = lookup_cached_layer(slot->alive_layers[j].item_id);
                    if (other && strcmp(other->data.item.type, req_type) == 0)
                        slot->alive_layers[j].active = false;
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
    LOG_INFO("[INTERACTION_BUBBLE] Dead-equip: item=%s active=%d\n", item_id, active);
}

const ObjectLayerState* interaction_bubble_get_alive_layers(
    const char *entity_id, int *out_count) {
    if (out_count) *out_count = 0;
    if (!entity_id || entity_id[0] == '\0') return NULL;
    InteractionBubbleSlot* slot = find_slot(entity_id);
    if (!slot || slot->alive_layer_count <= 0) return NULL;
    if (out_count) *out_count = slot->alive_layer_count;
    return slot->alive_layers;
}
