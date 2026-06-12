#include "modal_interact.h"

#include "dialogue_data.h"
#include "domain/local_player.h"
#include "game_state.h"
#include "interaction_bubble.h"
#include "inventory_modal.h"
#include "item_slot.h"
#include "modal.h"
#include "modal_dialogue.h"
#include "modal_notification.h"
#include "notification.h"
#include "object_layer.h"
#include "object_layers_management.h"
#include "quest_store.h"
#include "quest_metadata_cache.h"
#include "ui_button.h"
#include "util/log.h"

#include <raylib.h>
#include <stdio.h>
#include <string.h>

/* ── Tabs ─────────────────────────────────────────────────────────────── */

enum { MI_TAB_STACK = 0, MI_TAB_STATS, MI_TAB_ACTION, MI_TAB_COUNT };

static const char* MI_TAB_ICON[MI_TAB_COUNT]  = { "stack", "stats", "quest" };
static const char* MI_TAB_LABEL[MI_TAB_COUNT] = { "Stack", "Stats", "Quest" };

/* ── Module state ─────────────────────────────────────────────────────── */

static bool  s_open = false;
static float s_age  = 0.0f;
static int   s_tab  = MI_TAB_STACK;

static char  s_entity_id[64]    = {0};
static char  s_display_name[64] = {0};
static char  s_dlg_item[128]    = {0};
static bool  s_has_dialogue = false;
static bool  s_is_action_provider = false;
static Color s_border = { 80, 160, 220, 240 };

/* Authoritative quest code this NPC offers (from AOI), "" when none. The
 * action tab fetches its metadata by code via REST. */
static char  s_offer_code[64] = {0};

/* True while this modal holds a server interaction context (dlg_start sent on
 * open, freezing the player and binding the entity); released on close. */
static bool  s_dlg_context = false;


/* Reward slot hit-boxes captured during the action-tab draw so the click
 * handler can open the same read-only inspection the stack tab uses. */
#define MI_REWARD_SLOT_MAX 8
static Rectangle        s_reward_rects[MI_REWARD_SLOT_MAX];
static ObjectLayerState s_reward_ols[MI_REWARD_SLOT_MAX];
static int              s_reward_slot_count = 0;

/* Abandon-button hit-box, captured during the action-tab draw when an active
 * quest is shown so the click handler can drop it to the failed section. */
static Rectangle s_abandon_rect = { 0 };
static bool      s_abandon_visible = false;
static char      s_abandon_code[64] = { 0 };

/* Accept-button hit-box, captured when an acceptable offer is shown. */
static Rectangle s_accept_rect = { 0 };
static bool      s_accept_visible = false;

/* The paired modal_dialogue always opens alongside this modal; opened once
 * the (async) skin dialogue resolves, render-only when there is none. */
static bool  s_dialogue_opened = false;

/* Cached alive layers — snapshot at open so the stack stays visible even if
 * the entity leaves the AOI. */
static ObjectLayerState s_cached_layers[IBUBBLE_MAX_LAYERS];
static int              s_cached_layer_count = 0;

/* The JS overlay is open and we wait for it to close to reopen this modal. */
static bool  s_overlay_open = false;

/* ─────────────────────────────────────────────────────────────────────────
 *  EPHEMERAL SESSION DATA — survives navigating away to inspection modals
 *  and back, even when the entity leaves the AOI.  Once the modal session
 *  ends (the user closes it) this buffer is discarded.
 * ───────────────────────────────────────────────────────────────────────── */
#define ES_MAX_DEPTH 8

/* One per logical "push".  The top entry is the live one; a push stores the
 * current state so that popping restores it.  This lets us navigate to the
 * inventory inspector and back without the AOI leaving the entity. */
typedef struct {
    char  entity_id[64];
    char  display_name[64];
    char  dlg_item[128];
    bool  has_dialogue;
    bool  is_action_provider;
    Color border;
    ObjectLayerState layers[IBUBBLE_MAX_LAYERS];
    int   layer_count;
    int   tab;
} EpsSessionFrame;

static EpsSessionFrame s_es_stack[ES_MAX_DEPTH];
static int             s_es_depth = 0;

static void es_push(void) {
    if (s_es_depth >= ES_MAX_DEPTH) return;
    EpsSessionFrame* f = &s_es_stack[s_es_depth];
    strncpy(f->entity_id, s_entity_id, sizeof(f->entity_id) - 1);
    strncpy(f->display_name, s_display_name, sizeof(f->display_name) - 1);
    strncpy(f->dlg_item, s_dlg_item, sizeof(f->dlg_item) - 1);
    f->has_dialogue       = s_has_dialogue;
    f->is_action_provider = s_is_action_provider;
    f->border             = s_border;
    f->layer_count        = s_cached_layer_count;
    f->tab                = s_tab;
    if (f->layer_count > 0)
        memcpy(f->layers, s_cached_layers, sizeof(ObjectLayerState) * f->layer_count);
    s_es_depth++;
}

static void es_pop(void) {
    if (s_es_depth <= 0) return;
    s_es_depth--;
    const EpsSessionFrame* f = &s_es_stack[s_es_depth];
    strncpy(s_entity_id, f->entity_id, sizeof(s_entity_id) - 1);
    strncpy(s_display_name, f->display_name, sizeof(s_display_name) - 1);
    strncpy(s_dlg_item, f->dlg_item, sizeof(s_dlg_item) - 1);
    s_has_dialogue       = f->has_dialogue;
    s_is_action_provider = f->is_action_provider;
    s_border             = f->border;
    s_tab                = f->tab;
    s_cached_layer_count = f->layer_count;
    if (f->layer_count > 0)
        memcpy(s_cached_layers, f->layers, sizeof(ObjectLayerState) * f->layer_count);
}

static void es_clear(void) {
    s_es_depth = 0;
}

/* ── Layout ───────────────────────────────────────────────────────────── */

#define MI_PAD            18
#define MI_HEADER_H       46
#define MI_CLOSE_SZ       40
#define MI_TAB_H          40
#define MI_TAB_W          104
#define MI_TAB_GAP        4
#define MI_BAR_H          56
#define MI_BAR_BTN_H      40
#define MI_BAR_BTN_MAXW   150
#define MI_BAR_BTN_GAP    8
#define MI_SLOT_SZ        56
#define MI_SLOT_GAP       8
#define MI_FONT_NAME      22
#define MI_FONT_BTN       18
#define MI_FONT_LABEL     14
#define MI_FONT_STAT      14
#define MI_FONT_QUEST     13
#define MI_FONT_DESC      12
#define MI_FONT_REW       11
#define MI_REW_SLOT_SZ    32
#define MI_REW_SLOT_GAP   6

static const Color C_BTN        = {  24,  30,  48, 255 };
static const Color C_CONTENT    = {  60,  80, 130,  36 };
static const Color C_TAB_ACTIVE = {  70, 110, 175, 200 };
static const Color C_TEXT       = { 220, 220, 230, 240 };
static const Color C_TAB_DIM    = { 130, 140, 165, 220 };
static const Color C_LABEL      = { 150, 160, 190, 220 };
static const Color C_STAT       = { 120, 220, 140, 255 };
static const Color C_REW_LABEL  = { 255, 215,   0, 220 };
static const Color C_DESC_TEXT  = { 170, 180, 200, 220 };

/* ── Quest-tab visibility predicate ──────────────────────────────────── */

/* True when every prerequisite of `code` is completed. Optimistic while the
 * metadata is still loading so the offer gets a chance to resolve. */
static bool offer_prereqs_met(const char* code) {
    const QuestMetadataEntry* m = quest_metadata_cache_get(code);
    if (!m || QUEST_META_READY != m->state) return true;
    for (int i = 0; i < m->prerequisite_count; i++) {
        if (!quest_store_is_completed(m->prerequisites[i])) return false;
    }
    return true;
}

/* The quest tab is shown only when the player actually has an interaction with
 * it: this NPC grants a quest that is in progress or acceptable (prerequisites
 * met), OR an active quest's current step needs a talk with this NPC's skin.
 * A bare quest-talk NPC with nothing to offer or progress hides the tab. */
static bool quest_tab_visible(void) {
    if (!s_is_action_provider) return false;

    if (s_offer_code[0] != '\0') {
        const QuestEntry* q = quest_store_find(s_offer_code);
        if (q && QUEST_ACTIVE == q->status) return true;            /* in progress */
        if (q && QUEST_COMPLETED == q->status) return true;         /* completed feedback */
        if ((!q || QUEST_FAILED == q->status) && offer_prereqs_met(s_offer_code))
            return true;                                            /* acceptable */
    }

    if (s_dlg_item[0] != '\0') {
        int qc = quest_store_count(QUEST_ACTIVE);
        for (int qi = 0; qi < qc; qi++) {
            const QuestEntry* e = quest_store_get(QUEST_ACTIVE, qi);
            if (e && strstr(e->objectives, s_dlg_item)) return true;
        }
    }
    return false;
}

/* True when the paired dialogue is genuinely mission-relevant: this NPC has an
 * acceptable offer, or an active quest's current step needs a talk with it. The
 * dialogue's quest icon + yellow frame show only in these cases (an in-progress
 * quest whose current step is elsewhere does not count). */
static bool quest_dialogue_relevant(void) {
    if (!s_is_action_provider) return false;

    if (s_offer_code[0] != '\0') {
        const QuestEntry* q = quest_store_find(s_offer_code);
        if ((!q || QUEST_FAILED == q->status) && offer_prereqs_met(s_offer_code))
            return true; /* acceptable offer */
    }

    if (s_dlg_item[0] != '\0') {
        int qc = quest_store_count(QUEST_ACTIVE);
        for (int qi = 0; qi < qc; qi++) {
            const QuestEntry* e = quest_store_get(QUEST_ACTIVE, qi);
            if (e && strstr(e->objectives, s_dlg_item)) return true; /* talk step */
        }
    }
    return false;
}

static int tab_count(void) {
    if (!s_is_action_provider) return 2;
    return quest_tab_visible() ? 3 : 2;
}

/* Top half of the screen with generous padding all round. */
/* Top-half card. Its bottom edge meets the dialogue's top (MI_TOP_FRAC ==
 * DLG_TOP_FRAC) with a one-padding gap, so the two halves frame the screen
 * with matching margins. */
#define MI_TOP_FRAC 0.56f

static Rectangle card_rect(void) {
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();
    return (Rectangle){ (float)MI_PAD, (float)MI_PAD,
                        (float)(sw - 2 * MI_PAD), sh * MI_TOP_FRAC - MI_PAD * 1.5f };
}

static Rectangle close_rect(Rectangle card) {
    return (Rectangle){ card.x + card.width - MI_CLOSE_SZ - 8.0f, card.y + 3.0f,
                        (float)MI_CLOSE_SZ, (float)MI_CLOSE_SZ };
}

static Rectangle tab_rect(Rectangle card, int i) {
    float y = card.y + MI_HEADER_H;
    return (Rectangle){ card.x + MI_PAD + (float)i * (MI_TAB_W + MI_TAB_GAP), y,
                        (float)MI_TAB_W, (float)MI_TAB_H };
}

/* Content area between the tab strip and the bottom bar. */
static Rectangle content_rect(Rectangle card) {
    float top = card.y + MI_HEADER_H + MI_TAB_H + MI_PAD;
    float bot = card.y + card.height - MI_BAR_H - MI_PAD;
    return (Rectangle){ card.x + MI_PAD, top, card.width - 2 * MI_PAD, bot - top };
}

static Rectangle bar_rect(Rectangle card) {
    return (Rectangle){ card.x, card.y + card.height - MI_BAR_H, card.width, (float)MI_BAR_H };
}

/* Chat + Integration buttons: right-aligned in the bottom bar, each capped
 * at MI_BAR_BTN_MAXW wide. */
static void bar_buttons(Rectangle card, Rectangle* chat, Rectangle* integration) {
    Rectangle bar = bar_rect(card);
    float bw = (bar.width - 3 * MI_PAD) * 0.5f;
    if (bw > MI_BAR_BTN_MAXW) bw = MI_BAR_BTN_MAXW;
    float by = bar.y + (MI_BAR_H - MI_BAR_BTN_H) * 0.5f;
    float ix = bar.x + bar.width - MI_PAD - bw;
    *integration = (Rectangle){ ix, by, bw, (float)MI_BAR_BTN_H };
    *chat        = (Rectangle){ ix - MI_BAR_BTN_GAP - bw, by, bw, (float)MI_BAR_BTN_H };
}

static Rectangle slot_rect_in(Rectangle content, int i) {
    return (Rectangle){ content.x + (float)i * (MI_SLOT_SZ + MI_SLOT_GAP), content.y,
                        (float)MI_SLOT_SZ, (float)MI_SLOT_SZ };
}

static Rectangle rew_slot_rect(Rectangle content, int i, float y) {
    return (Rectangle){ content.x + (float)i * (MI_REW_SLOT_SZ + MI_REW_SLOT_GAP), y,
                        (float)MI_REW_SLOT_SZ, (float)MI_REW_SLOT_SZ };
}


/* ── Helpers ──────────────────────────────────────────────────────────── */

/* Refresh the snapshot from the live AOI, but never clear it: once an entity
 * leaves the AOI its layers stay isolated for the whole modal session (the
 * caller resets the count only when a *different* entity is opened). */
static void snapshot_alive_layers(void) {
    int lc = 0;
    const ObjectLayerState* layers = interaction_bubble_get_alive_layers(s_entity_id, &lc);
    if (layers && lc > 0) {
        int n = lc < IBUBBLE_MAX_LAYERS ? lc : IBUBBLE_MAX_LAYERS;
        memcpy(s_cached_layers, layers, sizeof(ObjectLayerState) * n);
        s_cached_layer_count = n;
    }
}

static Stats stack_stats(void) {
    Stats t = { 0 };
    ObjectLayersManager* mgr = obj_layers_mgr_get();
    for (int i = 0; i < s_cached_layer_count; i++) {
        ObjectLayer* ol = mgr ? lookup_cached_layer(s_cached_layers[i].item_id) : NULL;
        if (!ol) continue;
        t.effect       += ol->data.stats.effect;
        t.resistance   += ol->data.stats.resistance;
        t.agility      += ol->data.stats.agility;
        t.range        += ol->data.stats.range;
        t.intelligence += ol->data.stats.intelligence;
        t.utility      += ol->data.stats.utility;
    }
    return t;
}

/* Open the paired dialogue (bottom half). The client fetches dialogue at
 * /code/default-<itemId>, so the reported group is "default-<skin>". */
static void open_dialogue(const DialogueLine* lines, int count) {
    char code[96];
    snprintf(code, sizeof(code), "default-%s", s_dlg_item);
    modal_dialogue_open(s_entity_id, s_dlg_item, code,
                        MODAL_DIALOGUE_RENDER_ENTITY, lines, count);
    /* Yellow quest frame only when this dialogue actually matters for a mission
     * — an acceptable offer or an active quest-talk step — not for every NPC. */
    modal_dialogue_set_quest_style(quest_dialogue_relevant());
    s_dialogue_opened = true;
}

/* Accept the offered mission — the only path to start it. The server validates
 * the NPC offers the quest and the prerequisites are met. Reading the dialogue
 * never grants a quest; only this explicit request does. */
static void accept_mission(void) {
    local_player_request_quest_accept(s_entity_id);
}

static void open_overlay(int tab) {
    s_overlay_open = true;
    char entity[64];
    strncpy(entity, s_entity_id, sizeof(entity) - 1);
    entity[sizeof(entity) - 1] = '\0';
    interaction_bubble_open_js_overlay(entity, tab);
}

/* Re-open the interact modal from ephemeral session data rather than
 * re-snapshotting from the AOI — the entity may have left the bubble. */
static void modal_interact_reopen(void) {
    es_pop();
    s_open             = true;
    s_age              = 0.0f;
    s_dialogue_opened  = false;
    s_overlay_open     = false;

    /* The paired dialogue always appears alongside this modal. When the skin
     * has a default dialogue, open with its text (resolved async in update);
     * otherwise open render-only right away. */
    if (s_has_dialogue) dialogue_data_request(s_dlg_item);
    else                open_dialogue(NULL, 0);

    LOG_INFO("[MODAL_INTERACT] Reopen from ephemeral session: entity=%s layers=%d\n",
             s_entity_id, s_cached_layer_count);
}

void modal_interact_overlay_closed(void) {
    if (!s_overlay_open) return;
    s_overlay_open = false;
    modal_interact_reopen();
}

/* ── Public API ───────────────────────────────────────────────────────── */

void modal_interact_init(void) {
    s_open = false;
    s_overlay_open = false;
    es_clear();
}

void modal_interact_open(const char* entity_id, const char* display_name,
                         const char* dialogue_item_id, bool has_dialogue,
                         bool is_action_provider, Color border) {
    /* A different entity starts a fresh session → drop the prior snapshot.
     * Reopening the same entity (e.g. returning from item inspection) keeps
     * it, so the modal still renders even if the entity left the AOI. */
    bool same_entity = (0 == strcmp(s_entity_id, entity_id ? entity_id : ""));
    if (!same_entity) {
        s_cached_layer_count = 0;
        es_clear();
    }

    /* Save current state as an ephemeral session frame BEFORE overwriting,
     * so the re-open path can pop it back verbatim. */
    if (same_entity && s_cached_layer_count > 0) {
        es_push();
    } else {
        es_clear();
    }

    strncpy(s_entity_id, entity_id ? entity_id : "", sizeof(s_entity_id) - 1);
    s_entity_id[sizeof(s_entity_id) - 1] = '\0';
    strncpy(s_display_name, display_name ? display_name : "", sizeof(s_display_name) - 1);
    s_display_name[sizeof(s_display_name) - 1] = '\0';
    strncpy(s_dlg_item, dialogue_item_id ? dialogue_item_id : "", sizeof(s_dlg_item) - 1);
    s_dlg_item[sizeof(s_dlg_item) - 1] = '\0';

    s_has_dialogue       = has_dialogue && s_dlg_item[0] != '\0';
    s_is_action_provider = is_action_provider;
    s_border             = border;
    s_age                = 0.0f;
    s_dialogue_opened    = false;
    s_overlay_open       = false;
    s_open               = true;
    s_tab                = MI_TAB_STACK;

    /* Only snapshot layers from AOI when we don't already have a cached
     * copy from the ephemeral session.  This way, returning from the
     * inventory inspector reuses the same data we had before. */
    if (s_cached_layer_count == 0) {
        snapshot_alive_layers();
    }

    /* The paired dialogue always appears alongside this modal. When the skin
     * has a default dialogue, open with its text (resolved async in update);
     * otherwise open render-only right away. */
    if (s_has_dialogue) dialogue_data_request(s_dlg_item);
    else                open_dialogue(NULL, 0);

    /* The quest this NPC grants comes from the authoritative AOI binding (always
     * on the wire, position-independent). A grantor surfaces the quest tab even
     * when its per-player status now reads passive — e.g. an already-completed
     * quest the player can still review. */
    s_offer_code[0] = '\0';
    {
        const BotState* bot = game_state_find_bot(s_entity_id);
        if (bot && bot->grant_quest_code[0] != '\0') {
            strncpy(s_offer_code, bot->grant_quest_code, sizeof(s_offer_code) - 1);
            s_offer_code[sizeof(s_offer_code) - 1] = '\0';
            quest_metadata_cache_fetch(s_offer_code);
            s_is_action_provider = true;
        }
    }

    /* Establish the dialogue/interaction context on the server the moment the
     * modal opens (freeze + bind ActiveDialogueEntityID), so a quest-talk is
     * validated reliably even if the bot then wanders out of the AOI. Only for
     * a genuine quest interaction; review-only / non-action taps don't freeze. */
    s_dlg_context = quest_dialogue_relevant();
    if (s_dlg_context) {
        local_player_request_dialogue_start(s_entity_id, s_dlg_item);
    }

    LOG_INFO("[MODAL_INTERACT] Open: entity=%s action=%d layers=%d offer=%s\n",
             s_entity_id, s_is_action_provider ? 1 : 0, s_cached_layer_count, s_offer_code);
}

void modal_interact_close(void) {
    s_open = false;
    s_overlay_open = false;
    es_clear();
    if (modal_dialogue_is_open()) modal_dialogue_close();
    /* Release the interaction context/freeze established on open. Dropped by the
     * server if the dialogue already completed/cancelled it. */
    if (s_dlg_context) {
        local_player_request_dialogue_cancel(s_entity_id, s_dlg_item);
        s_dlg_context = false;
    }
}

bool modal_interact_is_open(void) { return s_open; }

void modal_interact_update(float dt) {
    if (!s_open) return;
    s_age += dt;

    if (!s_dialogue_opened) {
        const DialogueDataSet* d = dialogue_data_get(s_dlg_item);
        if (d && d->state == DLG_DATA_READY && d->line_count > 0) {
            open_dialogue(d->lines, d->line_count);
        } else if (d && (d->state == DLG_DATA_EMPTY || d->state == DLG_DATA_ERROR)) {
            open_dialogue(NULL, 0);
        }
    }
}

/* ── Tab content ──────────────────────────────────────────────────────── */

static void draw_stack_tab(Rectangle content) {
    ObjectLayersManager* olm = obj_layers_mgr_get();
    if (s_cached_layer_count == 0) {
        DrawText("No active items.", (int)content.x, (int)content.y, MI_FONT_LABEL, C_LABEL);
        return;
    }
    for (int i = 0; i < s_cached_layer_count; i++) {
        item_slot_draw(slot_rect_in(content, i), &s_cached_layers[i], olm);
    }
}

static void draw_stats_tab(Rectangle content) {
    Stats t = stack_stats();
    const char* names[6] = { "Effect", "Resistance", "Agility", "Range", "Intelligence", "Utility" };
    int values[6] = { t.effect, t.resistance, t.agility, t.range, t.intelligence, t.utility };
    float row_h = MI_FONT_STAT + 8.0f;
    float col_w = content.width * 0.5f;
    for (int i = 0; i < 6; i++) {
        float cx = content.x + (i % 2) * col_w;
        float cy = content.y + (i / 2) * row_h;
        DrawText(names[i], (int)cx, (int)cy, MI_FONT_STAT, C_LABEL);
        char val[16];
        snprintf(val, sizeof(val), "%d", values[i]);
        int vw = MeasureText(val, MI_FONT_STAT);
        DrawText(val, (int)(cx + col_w - vw - MI_PAD), (int)cy, MI_FONT_STAT, C_STAT);
    }
}

/* ── Quest tab: show the entity's active-quest description + rewards ──── */
static void draw_action_tab(Rectangle content, int mx, int my) {
    /* ── Resolve the quest to show ──────────────────────────────────────
     * Prefer the player's active progress for this NPC's offered quest;
     * otherwise show the NPC's offer (pre-accept) from the REST metadata
     * cache, keyed by the authoritative grant code delivered over AOI. */
    const char* offer_code = s_offer_code;

    const QuestEntry* active = NULL;
    bool is_completed = false;
    if (offer_code[0] != '\0') {
        const QuestEntry* q = quest_store_find(offer_code);
        /* Active → progress + Abandon; completed → review-only "Completed";
         * a failed/abandoned one reads as an offer again (Accept shown). */
        if (q && QUEST_ACTIVE == q->status) active = q;
        else if (q && QUEST_COMPLETED == q->status) is_completed = true;
    }
    if (!active && !is_completed && s_dlg_item[0] != '\0') {
        int quest_count = quest_store_count(QUEST_ACTIVE);
        for (int qi = 0; qi < quest_count; qi++) {
            const QuestEntry* e = quest_store_get(QUEST_ACTIVE, qi);
            if (e && strstr(e->objectives, s_dlg_item)) { active = e; break; }
        }
    }

    const char* q_code = "";
    const char* q_title = "";
    const char* q_desc = "";
    const char* q_active_step = "";
    const char* q_objectives = "";
    bool is_offer = false;

    if (active) {
        q_code = active->code;
        q_title = active->title;
        q_desc = active->description;
        q_active_step = active->active_step;
        q_objectives = active->objectives;
    } else if (offer_code[0] != '\0') {
        const QuestMetadataEntry* om = quest_metadata_cache_get(offer_code);
        if (om && QUEST_META_READY == om->state) {
            q_code = om->code;
            q_title = om->title;
            q_desc = om->description;
            is_offer = !is_completed; /* a completed quest is not offerable */
        }
    }

    /* ── Prerequisite gate (offer only; the server enforces it too) ───── */
    bool locked = false;
    char lock_reason[160] = {0};
    if (is_offer) {
        const QuestMetadataEntry* om = quest_metadata_cache_get(offer_code);
        for (int i = 0; om && i < om->prerequisite_count; i++) {
            if (quest_store_is_completed(om->prerequisites[i])) continue;
            locked = true;
            quest_metadata_cache_fetch(om->prerequisites[i]); /* warm its title */
            const QuestMetadataEntry* pm = quest_metadata_cache_get(om->prerequisites[i]);
            snprintf(lock_reason, sizeof(lock_reason), "Requires: %s",
                     (pm && pm->title[0]) ? pm->title : om->prerequisites[i]);
            break;
        }
    }
    /* Top action row — only an Abandon button for active quests. There is NO
     * Talk/Accept button: a quest-talk is accepted and its talk objectives
     * advance by reading the paired dialogue to the end (the server validates
     * only a fully-read dlg_complete), so missions are never one-tap completed. */
    float y = content.y + 6.0f;
    s_abandon_visible = false;
    s_accept_visible = false;
    const float btnw = 110.0f;
    Rectangle btn = { content.x + content.width - btnw, y, btnw, 30.0f };
    if (active) {
        UIButtonStyle abandon = { .text = "Abandon", .font_size = MI_FONT_BTN - 3,
                                  .bg = (Color){ 120, 44, 44, 255 }, .text_color = C_TEXT };
        ui_button_draw(btn, &abandon, ui_button_resolve_state(true, false, ui_button_hit(btn, mx, my)));
        s_abandon_rect = btn;
        s_abandon_visible = true;
        strncpy(s_abandon_code, q_code, sizeof(s_abandon_code) - 1);
        s_abandon_code[sizeof(s_abandon_code) - 1] = '\0';
    } else if (is_offer && !locked) {
        UIButtonStyle accept = { .text = "Accept", .icon_id = "quest", .font_size = MI_FONT_BTN - 3,
                                 .bg = (Color){ 30, 110, 70, 255 }, .text_color = C_TEXT };
        ui_button_draw(btn, &accept, ui_button_resolve_state(true, false, ui_button_hit(btn, mx, my)));
        s_accept_rect = btn;
        s_accept_visible = true;
    }
    y += 36.0f;

    if (q_title[0] == '\0' && q_desc[0] == '\0') {
        DrawText(offer_code[0] != '\0' ? "Loading mission..."
                                       : "Accept a mission from this NPC to see quest details.",
                 (int)content.x, (int)y, MI_FONT_QUEST, C_LABEL);
        return;
    }

    /* ── Quest title ───────────────────────────────────────────────── */
    DrawText(q_title, (int)content.x, (int)y, MI_FONT_QUEST, C_TEXT);
    y += MI_FONT_QUEST + 4;

    /* ── Completed banner ───────────────────────────────────────────── */
    if (is_completed) {
        DrawText("Completed", (int)content.x, (int)y, MI_FONT_DESC, (Color){ 110, 210, 130, 235 });
        y += MI_FONT_DESC + 6;
    }

    /* ── Locked banner ──────────────────────────────────────────────── */
    if (locked) {
        DrawText(lock_reason, (int)content.x, (int)y, MI_FONT_DESC, (Color){ 220, 120, 110, 230 });
        y += MI_FONT_DESC + 6;
    }

    /* ── Quest description ──────────────────────────────────────────── */
    char desc_buf[512];
    strncpy(desc_buf, q_desc, sizeof(desc_buf) - 1);
    desc_buf[sizeof(desc_buf) - 1] = '\0';
    int fs_d = MI_FONT_DESC;
    int max_w = (int)(content.width);
    /* Word-wrap the description */
    {
        char line_buf[256] = {0};
        char line_test[256];
        char* tok = strtok(desc_buf, " ");
        while (tok) {
            if (line_buf[0] == '\0')
                snprintf(line_test, sizeof(line_test), "%s", tok);
            else
                snprintf(line_test, sizeof(line_test), "%s %s", line_buf, tok);
            if (MeasureText(line_test, fs_d) > max_w && line_buf[0] != '\0') {
                DrawText(line_buf, (int)content.x, (int)y, fs_d, C_DESC_TEXT);
                y += fs_d + 2;
                snprintf(line_buf, sizeof(line_buf), "%s", tok);
            } else {
                snprintf(line_buf, sizeof(line_buf), "%s", line_test);
            }
            tok = strtok(NULL, " ");
        }
        if (line_buf[0] != '\0') {
            DrawText(line_buf, (int)content.x, (int)y, fs_d, C_DESC_TEXT);
            y += fs_d + 4;
        }
    }

    ObjectLayersManager* mgr = obj_layers_mgr_get();
    const QuestMetadataEntry* qm = quest_metadata_cache_get(q_code);

    /* ── Mission steps: a sectioned objective list with per-step status ──
     * For an accepted quest the active step (matched by description) is gold
     * with live counts, prior steps are done (green), later ones pending. The
     * pre-accept offer previews every step neutrally. */
    if (qm && QUEST_META_READY == qm->state && qm->step_count > 0) {
        DrawText("Objectives:", (int)content.x, (int)y, MI_FONT_LABEL, C_REW_LABEL);
        y += MI_FONT_LABEL + 4;

        int active_idx = -1;
        if (active && q_active_step[0] != '\0') {
            for (int i = 0; i < qm->step_count; i++) {
                if (0 == strcmp(qm->steps[i].description, q_active_step)) { active_idx = i; break; }
            }
            if (active_idx < 0) active_idx = 0;
        }

        for (int i = 0; i < qm->step_count; i++) {
            const QuestStepMeta* sm = &qm->steps[i];
            bool done    = (active_idx >= 0 && i < active_idx);
            bool current = (active_idx >= 0 && i == active_idx);
            Color step_col = done ? (Color){ 120, 200, 130, 230 }
                           : current ? (Color){ 220, 190, 70, 240 }
                                     : (active ? (Color){ 110, 116, 138, 180 } : C_DESC_TEXT);
            DrawCircle((int)(content.x + 4), (int)(y + MI_FONT_DESC / 2), 3.0f, step_col);
            DrawText(sm->description, (int)(content.x + 14), (int)y, MI_FONT_DESC, step_col);
            y += MI_FONT_DESC + 2;

            if (current && q_objectives[0] != '\0') {
                DrawText(q_objectives, (int)(content.x + 14), (int)y, MI_FONT_REW, C_LABEL);
                y += MI_FONT_REW + 3;
            } else {
                for (int o = 0; o < sm->objective_count; o++) {
                    const QuestObjectiveMeta* om = &sm->objectives[o];
                    char line[96];
                    snprintf(line, sizeof(line), "%s %s x%d", om->type, om->item_id, om->quantity);
                    DrawText(line, (int)(content.x + 14), (int)y, MI_FONT_REW, (Color){ 110, 116, 138, 180 });
                    y += MI_FONT_REW + 1;
                }
            }
            y += 3;
        }
        y += 4;
    }

    /* ── Rewards: render as item_slot icons from metadata cache ── */
    DrawText("Rewards:", (int)content.x, (int)y, MI_FONT_LABEL, C_REW_LABEL);
    y += MI_FONT_LABEL + 4;

    s_reward_slot_count = 0;
    if (qm && qm->state == QUEST_META_READY && qm->reward_count > 0) {
        int slot_count = qm->reward_count < MI_REWARD_SLOT_MAX ? qm->reward_count : MI_REWARD_SLOT_MAX;
        for (int ri = 0; ri < slot_count; ri++) {
            const QuestRewardMeta* rm = &qm->rewards[ri];
            ObjectLayerState ol = { 0 };
            strncpy(ol.item_id, rm->item_id, MAX_ID_LENGTH - 1);
            ol.active = true;
            ol.quantity = rm->quantity;
            Rectangle rr = rew_slot_rect(content, ri, y);
            item_slot_draw(rr, &ol, mgr);
            /* Cache the hit-box + payload so a tap opens the read-only inspector. */
            s_reward_rects[ri] = rr;
            s_reward_ols[ri] = ol;
        }
        s_reward_slot_count = slot_count;
    } else {
        DrawText("Complete this quest to claim your reward.",
                 (int)content.x, (int)y, MI_FONT_REW, C_LABEL);
    }
}

/* ── Draw ─────────────────────────────────────────────────────────────── */

void modal_interact_draw(void) {
    if (!s_open) return;
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();
    int mx = GetMouseX(), my = GetMouseY();

    modal_draw_overlay(sw, sh, s_age);
    Rectangle card = modal_scale_rect(card_rect(), modal_pop_scale(s_age));
    float a = modal_pop_alpha(s_age);
    Color bg = MODAL_PANEL_BG;
    bg.a = (unsigned char)(150 * a);
    DrawRectangleRec(card, bg);
    Color bc = s_border;
    bc.a = (unsigned char)(bc.a * a);
    DrawRectangleLinesEx(card, 1.0f, bc);

    /* Header */
    DrawRectangle((int)card.x, (int)card.y, (int)card.width, MI_HEADER_H,
                  (Color){ s_border.r, s_border.g, s_border.b, 40 });
    if (s_display_name[0] != '\0') {
        DrawText(s_display_name, (int)(card.x + MI_PAD),
                 (int)(card.y + (MI_HEADER_H - MI_FONT_NAME) / 2), MI_FONT_NAME, C_TEXT);
    }
    Rectangle xr = close_rect(card);
    UIButtonStyle close_btn = { .icon_id = "close-yellow", .no_fill = true };
    ui_button_draw(xr, &close_btn, ui_button_resolve_state(true, false, ui_button_hit(xr, mx, my)));

    /* Content panel — faint, mostly transparent so the world shows through;
     * just enough to delimit the active tab's content area. */
    Rectangle content = content_rect(card);
    Rectangle panel = { card.x + MI_PAD - 4, card.y + MI_HEADER_H + MI_TAB_H,
                        card.width - 2 * (MI_PAD - 4),
                        (card.y + card.height - MI_BAR_H) - (card.y + MI_HEADER_H + MI_TAB_H) };
    DrawRectangleRec(panel, C_CONTENT);

    /* Tab strip — inactive tabs are transparent with a dimmed icon+label;
     * the active tab gets a clearly visible fill so it reads as selected. */
    for (int t = 0; t < tab_count(); t++) {
        Rectangle r = tab_rect(card, t);
        UIButtonStyle tb;
        if (t == s_tab) {
            tb = (UIButtonStyle){ .text = MI_TAB_LABEL[t], .icon_id = MI_TAB_ICON[t],
                                  .font_size = MI_FONT_LABEL, .bg = C_TAB_ACTIVE,
                                  .text_color = C_TEXT, .rounded = true, .roundness = 0.25f };
        } else {
            tb = (UIButtonStyle){ .text = MI_TAB_LABEL[t], .icon_id = MI_TAB_ICON[t],
                                  .font_size = MI_FONT_LABEL, .no_fill = true,
                                  .text_color = C_TAB_DIM };
        }
        ui_button_draw(r, &tb, UI_BUTTON_NORMAL);
    }

    if (s_tab == MI_TAB_STACK)       draw_stack_tab(content);
    else if (s_tab == MI_TAB_STATS)  draw_stats_tab(content);
    else if (s_tab == MI_TAB_ACTION) draw_action_tab(content, mx, my);

    /* Fixed bottom bar with integration buttons (right-aligned). No background
     * fill — it shares the modal's panel so the modal's bottom border stays
     * visible and the chrome reads as one surface. */
    Rectangle chat, integration;
    bar_buttons(card, &chat, &integration);

    UIButtonStyle chat_btn = { .text = "Chat", .icon_id = "chat",
                               .font_size = MI_FONT_BTN, .bg = C_BTN, .text_color = C_TEXT };
    ui_button_draw(chat, &chat_btn, ui_button_resolve_state(true, false, ui_button_hit(chat, mx, my)));

    int unread = notification_count(NOTIF_CHAT, s_entity_id);
    if (unread > 0) {
        float br = 11.0f;
        float bx = chat.x + chat.width - br - 4.0f;
        float by = chat.y + br - 2.0f;
        DrawCircle((int)bx, (int)by, br, (Color){ 210, 60, 60, 240 });
        char txt[8];
        snprintf(txt, sizeof(txt), "%d", unread > 99 ? 99 : unread);
        int tw = MeasureText(txt, 11);
        DrawText(txt, (int)(bx - tw * 0.5f), (int)(by - 5.5f), 11, (Color){ 255, 255, 255, 245 });
    }

    UIButtonStyle integration_btn = { .text = "Integration", .icon_id = "reload",
                                      .font_size = MI_FONT_BTN, .bg = C_BTN, .text_color = C_TEXT };
    ui_button_draw(integration, &integration_btn,
                   ui_button_resolve_state(true, false, ui_button_hit(integration, mx, my)));
}

/* ── Click ────────────────────────────────────────────────────────────── */

bool modal_interact_handle_click(int mx, int my) {
    if (!s_open) return false;
    if (s_age < MODAL_POP_DURATION) return true;

    Rectangle card = card_rect();

    if (ui_button_hit(close_rect(card), mx, my)) {
        modal_interact_close();
        return true;
    }

    /* Tab strip */
    for (int t = 0; t < tab_count(); t++) {
        if (ui_button_hit(tab_rect(card, t), mx, my)) {
            s_tab = t;
            return true;
        }
    }

    /* Bottom bar integration buttons */
    Rectangle chat, integration;
    bar_buttons(card, &chat, &integration);
    if (ui_button_hit(chat, mx, my)) {
        notification_clear(NOTIF_CHAT, s_entity_id);
        open_overlay(INTERACT_OVERLAY_TAB_CHAT);
        return true;
    }
    if (ui_button_hit(integration, mx, my)) {
        open_overlay(INTERACT_OVERLAY_TAB_INTEGRATION);
        return true;
    }

    Rectangle content = content_rect(card);

    /* Stack tab: tap an item slot → read-only inspection.  Push the current
     * modal session onto the ephemeral stack so popping restores it. */
    if (s_tab == MI_TAB_STACK) {
        for (int i = 0; i < s_cached_layer_count; i++) {
            if (item_slot_hit(slot_rect_in(content, i), mx, my)) {
                ObjectLayerState ols = s_cached_layers[i];
                es_push();  /* save current session before navigating away */
                modal_interact_close();
                inventory_modal_open_external(&ols);
                inventory_modal_set_on_close(modal_interact_reopen);
                return true;
            }
        }
    }

    /* Action tab: tap a reward slot → read-only inspection (same as stack). */
    if (s_tab == MI_TAB_ACTION) {
        for (int i = 0; i < s_reward_slot_count; i++) {
            if (item_slot_hit(s_reward_rects[i], mx, my)) {
                ObjectLayerState ols = s_reward_ols[i];
                es_push();
                modal_interact_close();
                inventory_modal_open_external(&ols);
                inventory_modal_set_on_close(modal_interact_reopen);
                return true;
            }
        }
    }

    /* Action tab: Accept — grant the offered mission (no reading required). */
    if (s_tab == MI_TAB_ACTION && s_accept_visible && ui_button_hit(s_accept_rect, mx, my)) {
        accept_mission();
        return true;
    }

    /* Action tab: Abandon — drop the active quest to the failed section. */
    if (s_tab == MI_TAB_ACTION && s_abandon_visible && ui_button_hit(s_abandon_rect, mx, my)) {
        local_player_request_quest_abandon(s_abandon_code);
        modal_interact_close();
        return true;
    }

    if (!ui_button_hit(card, mx, my)) {
        modal_interact_close();
        return true;
    }

    return true;
}

/* ── Exported for draw_dialogue_sprite in modal_dialogue.c ───────────── */

const ObjectLayerState* modal_interact_get_cached_layers(int* out_count) {
    if (out_count) *out_count = s_cached_layer_count;
    return s_cached_layer_count > 0 ? s_cached_layers : NULL;
}
