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
#include "action_meta_cache.h"
#include "quest_store.h"
#include "quest_metadata_cache.h"
#include "ui_button.h"
#include "ui_toggle.h"
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

/* The bot's action code (from AOI). The action metadata (label, dialogue map,
 * source cell) is fetched by code via REST. */
static char  s_action_code[64] = {0};

/* The single most-relevant quest this NPC offers/relates to for the player,
 * resolved from the action's quests (active > acceptable > completed), plus the
 * dialogue code to show for it (from the action's per-quest dialogue map). */
static char  s_offer_code[64] = {0};
static char  s_offer_dialog[64] = {0};

/* True while this modal holds a server interaction context (dlg_start sent on
 * open, freezing the player and binding the entity); released on close. */
static bool  s_dlg_context = false;


/* Max reward icons rendered per mission card. */
#define MI_REWARD_SLOT_MAX 8

/* Multi-mission quest tab: one collapsible ui_toggle card per offered quest,
 * each with a per-card Accept/Abandon button captured during draw. */
#define MI_QUEST_MAX 8
static UIToggle  s_q_toggle[MI_QUEST_MAX];
static bool      s_q_toggle_init = false;
static Rectangle s_q_header[MI_QUEST_MAX]; /* full header bar — click toggles */
static Rectangle s_q_btn[MI_QUEST_MAX];
static int       s_q_btn_kind[MI_QUEST_MAX]; /* 0 none, 1 accept, 2 abandon */
static char      s_q_btn_code[MI_QUEST_MAX][64];
static int       s_q_count = 0;

/* Reward icon hit-boxes captured across all visible cards during the draw, so
 * the click handler can open the same read-only inspection the stack tab uses. */
static Rectangle        s_reward_rects[MI_QUEST_MAX * MI_REWARD_SLOT_MAX];
static ObjectLayerState s_reward_ols[MI_QUEST_MAX * MI_REWARD_SLOT_MAX];
static int              s_reward_slot_count = 0;

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

/* True when quest `code` is bound to action `am`'s own cell (so this NPC OFFERS
 * it, vs merely handling its talk step). Optimistic while metadata loads. */
static bool quest_offered_by_action(const char* code, const ActionMetaEntry* am) {
    const QuestMetadataEntry* qm = quest_metadata_cache_get(code);
    if (!qm || QUEST_META_READY != qm->state) return true;
    return qm->source_cell_x == am->source_cell_x && qm->source_cell_y == am->source_cell_y &&
           0 == strcmp(qm->source_map_code, am->source_map_code);
}

/* Resolve s_offer_code + s_offer_dialog from the fetched action metadata: the
 * most relevant quest the NPC offers (active > acceptable > completed). The
 * dialogue prefers an active quest whose current step needs a talk here. */
static void resolve_offer_from_action(void) {
    s_offer_code[0] = '\0';
    s_offer_dialog[0] = '\0';
    const ActionMetaEntry* am = action_meta_cache_get(s_action_code);
    if (!am || ACTION_META_READY != am->state) return;

    const char* pick_code = NULL;
    const char* pick_dlg = "";
    int pick_rank = 0; /* 1 acceptable, 2 completed, 3 active (highest) */
    for (int i = 0; i < am->quest_count; i++) {
        const char* qc = am->quests[i].quest_code;
        quest_metadata_cache_fetch(qc);
        if (!quest_offered_by_action(qc, am)) continue;
        const QuestEntry* q = quest_store_find(qc);
        int rank = 0;
        if (q && QUEST_ACTIVE == q->status) rank = 3;
        else if (q && QUEST_COMPLETED == q->status) rank = 2;
        else if ((!q || QUEST_FAILED == q->status) && offer_prereqs_met(qc)) rank = 1;
        if (rank > pick_rank) {
            pick_rank = rank;
            pick_code = qc;
            pick_dlg = am->quests[i].dialog_code;
        }
    }
    /* Talk-target: an active quest's current step needs a talk with this NPC's
     * skin (the quest may be offered elsewhere, e.g. report-back). Its dialogue
     * takes priority so reading it advances that talk objective. */
    for (int i = 0; i < am->quest_count; i++) {
        const char* qc = am->quests[i].quest_code;
        const QuestEntry* q = quest_store_find(qc);
        if (q && QUEST_ACTIVE == q->status && s_dlg_item[0] != '\0' &&
            strstr(q->objectives, s_dlg_item)) {
            pick_code = qc;
            pick_dlg = am->quests[i].dialog_code;
            break;
        }
    }
    if (pick_code) {
        strncpy(s_offer_code, pick_code, sizeof(s_offer_code) - 1);
        s_offer_code[sizeof(s_offer_code) - 1] = '\0';
        strncpy(s_offer_dialog, pick_dlg ? pick_dlg : "", sizeof(s_offer_dialog) - 1);
        s_offer_dialog[sizeof(s_offer_dialog) - 1] = '\0';
    }
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

/* Number of selectable tabs. The trailing quest tab only appears when this NPC
 * is mission-relevant; stack + stats are always present. */
static int tab_count(void) {
    return quest_tab_visible() ? MI_TAB_COUNT : MI_TAB_COUNT - 1;
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
    /* The dlg_complete code is the per-quest dialogue (so the server advances
     * the matching talk objective); falls back to the skin greeting. */
    char fallback[96];
    snprintf(fallback, sizeof(fallback), "default-%s", s_dlg_item);
    const char* code = s_offer_dialog[0] != '\0' ? s_offer_dialog : fallback;
    modal_dialogue_open(s_entity_id, s_dlg_item, code,
                        MODAL_DIALOGUE_RENDER_ENTITY, lines, count);
    /* Yellow quest frame only when this dialogue actually matters for a mission
     * — an acceptable offer or an active quest-talk step — not for every NPC. */
    modal_dialogue_set_quest_style(quest_dialogue_relevant());
    s_dialogue_opened = true;
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

    /* The bot carries its action code over AOI; fetch the action metadata
     * (label, dialogue map, source cell) and the metadata of each quest it
     * handles so the quest tab can list every mission. */
    s_action_code[0] = '\0';
    s_offer_code[0]  = '\0';
    {
        const BotState* bot = game_state_find_bot(s_entity_id);
        if (bot && bot->action_code[0] != '\0') {
            strncpy(s_action_code, bot->action_code, sizeof(s_action_code) - 1);
            s_action_code[sizeof(s_action_code) - 1] = '\0';
            action_meta_cache_fetch(s_action_code);
            s_is_action_provider = true;
        }
    }
    const ActionMetaEntry* am = action_meta_cache_get(s_action_code);
    if (am) {
        for (int i = 0; i < am->quest_count; i++) quest_metadata_cache_fetch(am->quests[i].quest_code);
    }

    /* The dialogue modal (paired with this interact modal) already sends its own
     * dlg_start on open and dlg_complete/cancel on finish — it owns the freeze/
     * unfreeze lifecycle. We do NOT send a redundant dlg_start here; doing so
     * creates a double-freeze that leaks the server-side ActiveDialogueEntityID
     * and breaks quest-talk validation when the bot leaves the AOI. The dialogue
     * modal's handshake is authoritative. */
    s_dlg_context = false;
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

    /* Action + quest metadata arrive async; re-resolve the offered quest each
     * frame until it settles. */
    resolve_offer_from_action();
    for (int i = 0; i < MI_QUEST_MAX; i++) ui_toggle_update(&s_q_toggle[i], dt);

    /* The per-quest dialog code can resolve after the dialogue already opened;
     * keep the open dialogue's dlg_complete code in sync so a quest-talk match
     * doesn't depend on metadata having loaded before the first read. */
    if (s_offer_dialog[0] != '\0') modal_dialogue_set_dialog_code(s_offer_dialog);

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
/* Collect the quest codes this NPC OFFERS (its action's quests bound to the
 * action's own cell). Returns the count. */
static int offered_quest_codes(char out[][64], int max) {
    int n = 0;
    const ActionMetaEntry* am = action_meta_cache_get(s_action_code);
    if (!am || ACTION_META_READY != am->state) return 0;
    for (int i = 0; i < am->quest_count && n < max; i++) {
        const char* qc = am->quests[i].quest_code;
        quest_metadata_cache_fetch(qc);
        if (!quest_offered_by_action(qc, am)) continue;
        strncpy(out[n], qc, 63);
        out[n][63] = '\0';
        n++;
    }
    return n;
}

/* Word-wrap helper for card bodies; returns the y after drawing. */
static float mi_wrap(const char* text, float x, float y, int maxw, int font, Color col) {
    if (!text || text[0] == '\0') return y;
    char buf[512];
    strncpy(buf, text, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    char line[512] = { 0 }, test[512];
    char* tok = strtok(buf, " ");
    while (tok) {
        if (line[0] == '\0') snprintf(test, sizeof(test), "%s", tok);
        else                 snprintf(test, sizeof(test), "%s %s", line, tok);
        if (MeasureText(test, font) > maxw && line[0] != '\0') {
            DrawText(line, (int)x, (int)y, font, col);
            y += font + 2;
            snprintf(line, sizeof(line), "%s", tok);
        } else {
            snprintf(line, sizeof(line), "%s", test);
        }
        tok = strtok(NULL, " ");
    }
    if (line[0] != '\0') { DrawText(line, (int)x, (int)y, font, col); y += font + 2; }
    return y;
}

/* One collapsible mission card. Advances *y; records the per-card primary
 * button (Accept / Abandon) so the click handler can act on it. */
static void draw_quest_card(int slot, const char* code, Rectangle content,
                            float* y, int mx, int my) {
    quest_metadata_cache_fetch(code);
    const QuestEntry* q = quest_store_find(code);
    const QuestMetadataEntry* qm = quest_metadata_cache_get(code);

    bool active    = q && QUEST_ACTIVE == q->status;
    bool completed = q && QUEST_COMPLETED == q->status;
    bool locked    = false;
    if (!active && !completed && qm && QUEST_META_READY == qm->state) {
        for (int i = 0; i < qm->prerequisite_count; i++) {
            if (!quest_store_is_completed(qm->prerequisites[i])) { locked = true; break; }
        }
    }
    bool acceptable = !active && !completed && !locked;

    const char* title = (q && q->title[0]) ? q->title
                      : (qm && qm->title[0]) ? qm->title : code;
    Color statusc = active ? (Color){ 220, 190, 70, 255 }
                  : completed ? (Color){ 110, 210, 130, 235 }
                  : locked ? (Color){ 210, 120, 110, 230 }
                           : (Color){ 120, 200, 140, 235 };
    const char* statusw = active ? "Active" : completed ? "Completed"
                        : locked ? "Locked" : "Available";

    UIToggle* tg = &s_q_toggle[slot];
    float hx = content.x, hw = content.width;
    Rectangle header = { hx, *y, hw, 26.0f };
    s_q_header[slot] = header;
    ui_toggle_set_anchor(tg, (Rectangle){ hx + 4, *y + 4, 18, 18 });
    DrawRectangleRounded(header, 0.18f, 4, (Color){ 30, 34, 52, 205 });
    ui_toggle_draw(tg);
    DrawCircle((int)(hx + 30), (int)(*y + 13), 3.5f, statusc);
    DrawText(title, (int)(hx + 40), (int)(*y + (26 - MI_FONT_QUEST) / 2), MI_FONT_QUEST, C_TEXT);
    int sww = MeasureText(statusw, MI_FONT_REW);
    DrawText(statusw, (int)(hx + hw - sww - 8), (int)(*y + (26 - MI_FONT_REW) / 2), MI_FONT_REW, statusc);
    *y += 26 + 4;

    s_q_btn_kind[slot] = 0;
    if (!tg->expanded) { *y += 2; return; }

    float ix = hx + 16;
    int   iw = (int)(hw - 24);

    if (qm && QUEST_META_READY == qm->state) {
        *y = mi_wrap(qm->description, ix, *y, iw, MI_FONT_DESC, C_DESC_TEXT);
        *y += 4;
        if (qm->reward_count > 0) {
            DrawText("Reward:", (int)ix, (int)*y, MI_FONT_REW, C_REW_LABEL);
            float rx = ix + MeasureText("Reward:", MI_FONT_REW) + 8;
            ObjectLayersManager* mgr = obj_layers_mgr_get();
            for (int r = 0; r < qm->reward_count && r < MI_REWARD_SLOT_MAX; r++) {
                ObjectLayerState ol = { 0 };
                strncpy(ol.item_id, qm->rewards[r].item_id, MAX_ID_LENGTH - 1);
                ol.active = true;
                ol.quantity = qm->rewards[r].quantity;
                Rectangle rr = { rx + r * (MI_REW_SLOT_SZ + MI_REW_SLOT_GAP), *y - 4,
                                 MI_REW_SLOT_SZ, MI_REW_SLOT_SZ };
                item_slot_draw(rr, &ol, mgr);
                if (s_reward_slot_count < MI_QUEST_MAX * MI_REWARD_SLOT_MAX) {
                    s_reward_rects[s_reward_slot_count] = rr;
                    s_reward_ols[s_reward_slot_count]   = ol;
                    s_reward_slot_count++;
                }
            }
            *y += MI_REW_SLOT_SZ + 2;
        }
    } else {
        DrawText("Loading...", (int)ix, (int)*y, MI_FONT_DESC, C_LABEL);
        *y += MI_FONT_DESC + 2;
    }

    if (active || acceptable) {
        Rectangle btn = { hx + hw - 100, *y, 96, 26 };
        UIButtonStyle st = active
            ? (UIButtonStyle){ .text = "Abandon", .font_size = MI_FONT_BTN - 4,
                               .bg = (Color){ 120, 44, 44, 255 }, .text_color = C_TEXT }
            : (UIButtonStyle){ .text = "Accept", .icon_id = "quest", .font_size = MI_FONT_BTN - 4,
                               .bg = (Color){ 30, 110, 70, 255 }, .text_color = C_TEXT };
        ui_button_draw(btn, &st, ui_button_resolve_state(true, false, ui_button_hit(btn, mx, my)));
        s_q_btn[slot] = btn;
        s_q_btn_kind[slot] = active ? 2 : 1;
        strncpy(s_q_btn_code[slot], code, 63);
        s_q_btn_code[slot][63] = '\0';
        *y += 30;
    } else if (locked) {
        DrawText("Prerequisite not met.", (int)ix, (int)*y, MI_FONT_REW,
                 (Color){ 210, 120, 110, 220 });
        *y += MI_FONT_REW + 4;
    }
    *y += 4;
}

/* Quest tab: a collapsible card per mission this NPC offers. */
static void draw_action_tab(Rectangle content, int mx, int my) {
    if (!s_q_toggle_init) {
        for (int i = 0; i < MI_QUEST_MAX; i++)
            ui_toggle_init(&s_q_toggle[i], (Rectangle){ 0, 0, 18, 18 }, 0 == i, UI_TOGGLE_CHEVRON_DOWN);
        s_q_toggle_init = true;
    }
    s_reward_slot_count = 0;

    char codes[MI_QUEST_MAX][64];
    s_q_count = offered_quest_codes(codes, MI_QUEST_MAX);

    float y = content.y + 4.0f;
    if (0 == s_q_count) {
        const ActionMetaEntry* am = action_meta_cache_get(s_action_code);
        DrawText((am && ACTION_META_READY == am->state) ? "No missions available here."
                                                        : "Loading...",
                 (int)content.x, (int)y, MI_FONT_QUEST, C_LABEL);
        return;
    }
    DrawText("Missions", (int)content.x, (int)y, MI_FONT_LABEL, C_REW_LABEL);
    y += MI_FONT_LABEL + 6;
    for (int i = 0; i < s_q_count; i++) {
        draw_quest_card(i, codes[i], content, &y, mx, my);
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

    /* Action tab: mission cards — Accept/Abandon, reward inspect, or toggle
     * expand by clicking anywhere on the header bar. */
    if (s_tab == MI_TAB_ACTION) {
        for (int i = 0; i < s_q_count; i++) {
            if (0 == s_q_btn_kind[i] || !ui_button_hit(s_q_btn[i], mx, my)) continue;
            if (1 == s_q_btn_kind[i]) {
                local_player_request_quest_accept(s_entity_id, s_q_btn_code[i]);
            } else {
                local_player_request_quest_abandon(s_q_btn_code[i]);
            }
            return true;
        }
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
        for (int i = 0; i < s_q_count; i++) {
            if (CheckCollisionPointRec((Vector2){ (float)mx, (float)my }, s_q_header[i])) {
                s_q_toggle[i].expanded = !s_q_toggle[i].expanded;
                return true;
            }
        }
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
