#include "modal_interact.h"
#include "text.h"

#include "dialogue_data.h"
#include "domain/local_player.h"
#include "domain/viewport.h"
#include "game_state.h"
#include "world_types.h"
#include "interaction_bubble.h"
#include "inventory_bar.h"
#include "inventory_modal.h"
#include "item_slot.h"
#include "modal.h"
#include "modal_dialogue.h"
#include "modal_notification.h"
#include "notification.h"
#include "object_layer.h"
#include "object_layers_management.h"
#include "quest_progress_store.h"
#include "quest_cache.h"
#include "ui_button.h"
#include "ui_scroll.h"
#include "ui_toggle.h"
#include "util/log.h"

#include <raylib.h>
#include <stdio.h>
#include <string.h>

/* ── Tabs ─────────────────────────────────────────────────────────────── */

enum { MI_TAB_STACK = 0, MI_TAB_STATS, MI_TAB_QUEST, MI_TAB_COUNT };

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
/* Per-player interaction capability bitmask (INTERACTION_FLAG_*), from AOI.
 * The action bit marks a pending action-talk-quest; the quest bit enables the
 * Quest tab. */
static uint8_t s_interaction_flags = 0;
static Color s_border = { 80, 160, 220, 240 };

/* Per-player pending action-talk-quest dialogue code (from AOI), "" when none.
 * When set, the paired dialogue shows this mapped dialogue (quest-framed) in
 * place of the default greeting. */
static char  s_action_dialog_code[64] = {0};

/* Authoritative quest codes this NPC provides to the player (from AOI). The
 * Quest tab renders one mission card per code; metadata is fetched by code. */
static char  s_quest_codes[BOT_QUEST_CODES_MAX][64];
static int   s_quest_code_count = 0;

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
static UIScroll  s_q_scroll;
static float     s_q_content_height = 0.0f;

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

static void handle_quest_click(int mx, int my);

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
    char  action_dialog_code[64];
    bool  has_dialogue;
    uint8_t interaction_flags;
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
    strncpy(f->action_dialog_code, s_action_dialog_code, sizeof(f->action_dialog_code) - 1);
    f->has_dialogue       = s_has_dialogue;
    f->interaction_flags  = s_interaction_flags;
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
    strncpy(s_action_dialog_code, f->action_dialog_code, sizeof(s_action_dialog_code) - 1);
    s_has_dialogue       = f->has_dialogue;
    s_interaction_flags  = f->interaction_flags;
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

/* Quest tab desktop enlargement. The mission cards, reward slots, toggle
 * chevrons and Accept/Abandon button read too small at the base sizes above
 * on a full-size monitor; bumped up on desktop only via the mi_* accessors
 * below. Mobile keeps the base sizing untouched (already scaled down by
 * text.c's TEXT_MOBILE_FONT_SCALE). */
#define MI_FONT_QLABEL_DESKTOP   18
#define MI_FONT_QUEST_DESKTOP    17
#define MI_FONT_DESC_DESKTOP     15
#define MI_FONT_REW_DESKTOP      14
#define MI_REW_SLOT_SZ_DESKTOP   44.0f
#define MI_REW_SLOT_GAP_DESKTOP  8.0f
#define MI_Q_CHEVRON_DESKTOP     28.0f
#define MI_Q_BTN_W_DESKTOP       124.0f
#define MI_Q_BTN_H_DESKTOP       34.0f
#define MI_FONT_QBTN_DESKTOP     16

static inline int   mi_font_qlabel(void) { return viewport_is_mobile() ? MI_FONT_LABEL : MI_FONT_QLABEL_DESKTOP; }
static inline int   mi_font_quest(void)  { return viewport_is_mobile() ? MI_FONT_QUEST : MI_FONT_QUEST_DESKTOP; }
static inline int   mi_font_desc(void)   { return viewport_is_mobile() ? MI_FONT_DESC  : MI_FONT_DESC_DESKTOP; }
static inline int   mi_font_rew(void)    { return viewport_is_mobile() ? MI_FONT_REW   : MI_FONT_REW_DESKTOP; }
static inline float mi_rew_slot_sz(void) { return viewport_is_mobile() ? (float)MI_REW_SLOT_SZ  : MI_REW_SLOT_SZ_DESKTOP; }
static inline float mi_rew_slot_gap(void){ return viewport_is_mobile() ? (float)MI_REW_SLOT_GAP : MI_REW_SLOT_GAP_DESKTOP; }
static inline float mi_q_chevron(void)   { return viewport_is_mobile() ? UI_TOGGLE_HDR_CHEVRON : MI_Q_CHEVRON_DESKTOP; }
static inline float mi_pad(void)          { return viewport_is_mobile() ? 8.0f : (float)MI_PAD; }
static inline float mi_header_h(void)     { return viewport_is_mobile() ? 36.0f : (float)MI_HEADER_H; }
static inline float mi_close_sz(void)     { return viewport_is_mobile() ? 32.0f : (float)MI_CLOSE_SZ; }
static inline float mi_tab_h(void)        { return viewport_is_mobile() ? 32.0f : (float)MI_TAB_H; }
static inline float mi_tab_w(Rectangle card) {
    if (!viewport_is_mobile()) return (float)MI_TAB_W;
    float available = (card.width - 2.0f * mi_pad() - 2.0f * MI_TAB_GAP) / MI_TAB_COUNT;
    return available < 90.0f ? available : 90.0f;
}
static inline float mi_bar_h(void)        { return viewport_is_mobile() ? 48.0f : (float)MI_BAR_H; }
static inline float mi_bar_btn_h(void)    { return viewport_is_mobile() ? 34.0f : (float)MI_BAR_BTN_H; }
static inline float mi_bar_btn_maxw(void) { return viewport_is_mobile() ? 128.0f : (float)MI_BAR_BTN_MAXW; }
static inline int   mi_font_name(void)    { return viewport_is_mobile() ? 18 : MI_FONT_NAME; }
static inline int   mi_font_label(void)   { return viewport_is_mobile() ? 12 : MI_FONT_LABEL; }
static inline int   mi_font_btn(void)     { return viewport_is_mobile() ? 14 : MI_FONT_BTN; }

static const Color C_BTN        = {  24,  30,  48, 255 };
static const Color C_CONTENT    = {  60,  80, 130,  36 };
static const Color C_TAB_ACTIVE = {  70, 110, 175, 200 };
static const Color C_TEXT       = { 220, 220, 230, 240 };
static const Color C_TAB_DIM    = { 130, 140, 165, 220 };
static const Color C_LABEL      = { 150, 160, 190, 220 };
static const Color C_STAT       = { 120, 220, 140, 255 };
static const Color C_REW_LABEL  = { 255, 215,   0, 220 };
static const Color C_DESC_TEXT  = { 170, 180, 200, 220 };

/* ── Capability tabs ─────────────────────────────────────────────────── */

/* Stack and Stats always show; Quest appears whenever the NPC surfaces any
 * quest codes — including completed feedback, which carries no attention icon
 * (the quest capability bit lights only for actionable content). There is no
 * Action tab: a pending action-talk-quest needs no tab — the player just taps
 * the paired modal dialogue to advance it. */
static bool quest_tab_visible(void) {
    return s_quest_code_count > 0;
}

/* Fill `out` with the visible tab IDs in strip order; returns the count. */
static int visible_tabs(int out[MI_TAB_COUNT]) {
    int n = 0;
    out[n++] = MI_TAB_STACK;
    out[n++] = MI_TAB_STATS;
    if (quest_tab_visible())  out[n++] = MI_TAB_QUEST;
    return n;
}

/* Top half of the screen with generous padding all round. */
/* Top-half card. Its bottom edge meets the dialogue's top (MI_TOP_FRAC ==
 * DLG_TOP_FRAC) with a one-padding gap, so the two halves frame the screen
 * with matching margins. */
#define MI_TOP_FRAC 0.56f

static Rectangle card_rect(void) {
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();
    float pad = mi_pad();
    float hidden_bar_h = inventory_bar_full_height() - inventory_bar_visible_height();
    float bottom = sh * MI_TOP_FRAC - pad * 0.5f + hidden_bar_h;
    float max_bottom = sh - inventory_bar_visible_height() - pad;
    if (bottom > max_bottom) bottom = max_bottom;
    return (Rectangle){ pad, pad, (float)sw - 2.0f * pad, bottom - pad };
}

static Rectangle close_rect(Rectangle card) {
    float size = mi_close_sz();
    return (Rectangle){ card.x + card.width - size - mi_pad() * 0.5f, card.y + 2.0f,
                        size, size };
}

static Rectangle tab_rect(Rectangle card, int i) {
    float y = card.y + mi_header_h();
    float width = mi_tab_w(card);
    return (Rectangle){ card.x + mi_pad() + (float)i * (width + MI_TAB_GAP), y,
                        width, mi_tab_h() };
}

/* Content area between the tab strip and the bottom bar. */
static Rectangle content_rect(Rectangle card) {
    float pad = mi_pad();
    float top = card.y + mi_header_h() + mi_tab_h() + pad;
    float bot = card.y + card.height - mi_bar_h() - pad;
    return (Rectangle){ card.x + pad, top, card.width - 2.0f * pad, bot - top };
}

static Rectangle bar_rect(Rectangle card) {
    float height = mi_bar_h();
    return (Rectangle){ card.x, card.y + card.height - height, card.width, height };
}

/* Chat + Integration buttons: right-aligned in the bottom bar, each capped
 * at MI_BAR_BTN_MAXW wide. */
static void bar_buttons(Rectangle card, Rectangle* chat, Rectangle* integration) {
    Rectangle bar = bar_rect(card);
    float pad = mi_pad();
    float button_h = mi_bar_btn_h();
    float bw = (bar.width - 3.0f * pad) * 0.5f;
    if (bw > mi_bar_btn_maxw()) bw = mi_bar_btn_maxw();
    float by = bar.y + (bar.height - button_h) * 0.5f;
    float ix = bar.x + bar.width - pad - bw;
    *integration = (Rectangle){ ix, by, bw, button_h };
    *chat        = (Rectangle){ ix - MI_BAR_BTN_GAP - bw, by, bw, button_h };
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

/* The active dialogue key: a pending action-talk-quest dialogue code (shown
 * quest-framed in place of the greeting) when present, else the skin greeting. */
static const char* active_dlg_key(void) {
    return s_action_dialog_code[0] != '\0' ? s_action_dialog_code : s_dlg_item;
}

/* Open the paired dialogue (bottom half). A pending action-talk-quest shows its
 * mapped dialogue by full code; otherwise the entity's "default-<skin>" greeting.
 * The server validates talk objectives by the NPC's skin, not this code, so the
 * code is reported on dlg_complete only for traceability. */
static void open_dialogue(const DialogueLine* lines, int count) {
    char code[96];
    if (s_action_dialog_code[0] != '\0')
        snprintf(code, sizeof(code), "%s", s_action_dialog_code);
    else
        snprintf(code, sizeof(code), "default-%s", s_dlg_item);
    modal_dialogue_open(s_entity_id, s_dlg_item, code,
                        MODAL_DIALOGUE_RENDER_ENTITY, lines, count);
    /* Yellow quest frame only for a pending action-talk-quest — not for every NPC
     * and not merely because the NPC offers a quest. */
    modal_dialogue_set_quest_style(s_action_dialog_code[0] != '\0');
    s_dialogue_opened = true;
}

/* Kick off the fetch for the active dialogue (pending quest dialogue by code, or
 * the skin greeting). Render-only when the skin has no greeting and there is no
 * pending dialogue. */
static void request_active_dialogue(void) {
    if (s_action_dialog_code[0] != '\0') dialogue_data_request_code(s_action_dialog_code);
    else if (s_has_dialogue)             dialogue_data_request(s_dlg_item);
    else                                 open_dialogue(NULL, 0);
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

    /* The paired dialogue always appears alongside this modal; its key (pending
     * quest dialogue or skin greeting) was restored by es_pop. Text resolves async
     * in update; render-only opens right away. */
    request_active_dialogue();

    LOG_INFO("[MODAL_INTERACT] Reopen from ephemeral session: entity=%s layers=%d\n",
             s_entity_id, s_cached_layer_count);
}

void modal_interact_overlay_closed(void) {
    if (!s_overlay_open) return;
    s_overlay_open = false;
    modal_interact_reopen();
}

void modal_interact_stack_player_item(int inv_idx) {
    if (!s_open) return;
    es_push();
    modal_interact_close();
    inventory_modal_open(inv_idx);
    inventory_modal_set_on_close(modal_interact_reopen);
}

/* ── Public API ───────────────────────────────────────────────────────── */

void modal_interact_init(void) {
    s_open = false;
    s_overlay_open = false;
    ui_scroll_reset(&s_q_scroll);
    s_q_content_height = 0.0f;
    es_clear();
}

void modal_interact_open(const char* entity_id, const char* display_name,
                         const char* dialogue_item_id, bool has_dialogue,
                         uint8_t interaction_flags, Color border) {
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
    s_interaction_flags  = interaction_flags;
    s_border             = border;
    s_age                = 0.0f;
    s_dialogue_opened    = false;
    s_overlay_open       = false;
    s_open               = true;
    s_tab                = MI_TAB_STACK;
    ui_scroll_reset(&s_q_scroll);
    s_q_content_height = 0.0f;

    /* Only snapshot layers from AOI when we don't already have a cached
     * copy from the ephemeral session.  This way, returning from the
     * inventory inspector reuses the same data we had before. */
    if (s_cached_layer_count == 0) {
        snapshot_alive_layers();
    }

    /* The bot carries, per player, the capability bitmask, the authoritative quest
     * codes it provides, and any pending action-talk-quest dialogue code. Read
     * these BEFORE requesting the dialogue, since the pending code decides which
     * dialogue (and frame style) the paired panel opens. */
    s_action_dialog_code[0] = '\0';
    s_quest_code_count = 0;
    {
        const BotState* bot = game_state_find_bot(s_entity_id);
        if (bot) {
            s_interaction_flags = bot->interaction_flags;
            strncpy(s_action_dialog_code, bot->action_dialog_code, sizeof(s_action_dialog_code) - 1);
            s_action_dialog_code[sizeof(s_action_dialog_code) - 1] = '\0';
            for (int i = 0; i < bot->quest_code_count && i < BOT_QUEST_CODES_MAX; i++) {
                strncpy(s_quest_codes[s_quest_code_count], bot->quest_codes[i], 63);
                s_quest_codes[s_quest_code_count][63] = '\0';
                quest_cache_fetch(s_quest_codes[s_quest_code_count]);
                s_quest_code_count++;
            }
        }
    }

    /* The paired dialogue always appears alongside this modal: a pending quest
     * dialogue (quest-framed) when present, else the skin greeting, else
     * render-only. Text resolves async in update. */
    request_active_dialogue();

    /* The dialogue modal (paired with this interact modal) already sends its own
     * dlg_start on open and dlg_complete/cancel on finish — it owns the freeze/
     * unfreeze lifecycle. We do NOT send a redundant dlg_start here; doing so
     * creates a double-freeze that leaks the server-side ActiveDialogueEntityID
     * and breaks quest-talk validation when the bot leaves the AOI. The dialogue
     * modal's handshake is authoritative. */
    s_dlg_context = false;
    LOG_INFO("[MODAL_INTERACT] Open: entity=%s flags=0x%x layers=%d quests=%d\n",
             s_entity_id, s_interaction_flags, s_cached_layer_count, s_quest_code_count);
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

float modal_interact_layout_bottom(void) {
    if (!s_open) return 0.0f;
    Rectangle card = card_rect();
    return card.y + card.height;
}

bool modal_interact_handle_wheel(float wheel_delta) {
    if (!s_open || s_age < MODAL_POP_DURATION || s_tab != MI_TAB_QUEST) return false;
    return ui_scroll_on_wheel(&s_q_scroll, content_rect(card_rect()),
                              s_q_content_height, wheel_delta);
}

void modal_interact_update(float dt) {
    if (!s_open) return;
    s_age += dt;

    for (int i = 0; i < MI_QUEST_MAX; i++) ui_toggle_update(&s_q_toggle[i], dt);

    if (s_tab == MI_TAB_QUEST) {
        ui_scroll_update(&s_q_scroll, content_rect(card_rect()), s_q_content_height, dt);
        int click_x, click_y;
        if (ui_scroll_take_click(&s_q_scroll, &click_x, &click_y)) {
            handle_quest_click(click_x, click_y);
            if (!s_open) return;
        }
    }

    if (!s_dialogue_opened) {
        const DialogueDataSet* d = dialogue_data_get(active_dlg_key());
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
        DrawText(val, (int)(cx + col_w - vw - mi_pad()), (int)cy, MI_FONT_STAT, C_STAT);
    }
}

/* ── Quest tab: show the entity's active-quest description + rewards ──── */

static int quest_active_step_index(const QuestMetadataEntry* metadata,
                                   const QuestProgressEntry* progress) {
    if (NULL == metadata || NULL == progress || '\0' == progress->active_step[0]) return 0;
    for (int i = 0; i < metadata->step_count; i++) {
        const QuestStepMeta* step = &metadata->steps[i];
        if (0 == strcmp(step->id, progress->active_step) ||
            0 == strcmp(step->description, progress->active_step)) {
            return i;
        }
    }
    return 0;
}

static const char* quest_objective_verb(const char* type) {
    if (type && 0 == strcmp(type, "collect")) return "Collect";
    if (type && 0 == strcmp(type, "kill"))    return "Defeat";
    if (type && 0 == strcmp(type, "talk"))    return "Talk to";
    return "Complete";
}

static float draw_quest_step_objectives(const QuestStepMeta* step,
                                        const char* progress, int x, float y,
                                        int width, int font, Color color) {
    if (progress && '\0' != progress[0]) {
        char line[QUEST_OBJECTIVES_MAX + 16];
        snprintf(line, sizeof(line), "Objective: %s", progress);
        return (float)text_wrap(line, x, (int)y, width, font, color, false, true);
    }
    if (NULL == step) return 0.0f;

    float used = 0.0f;
    for (int i = 0; i < step->objective_count; i++) {
        const QuestObjectiveMeta* objective = &step->objectives[i];
        char line[QUEST_CACHE_ITEM_MAX + 40];
        snprintf(line, sizeof(line), "Objective: %s %s x%d",
                 quest_objective_verb(objective->type), objective->item_id,
                 objective->quantity);
        int height = text_wrap(line, x, (int)(y + used), width, font, color, false, true);
        used += (float)height;
    }
    return used;
}

static void draw_quest_steps(const QuestMetadataEntry* metadata,
                             const QuestProgressEntry* progress, bool active,
                             bool completed, bool acceptable, float x, float* y,
                             int width, int step_font, int objective_font) {
    if (NULL == metadata || metadata->step_count <= 0) return;

    DrawText("Steps", (int)x, (int)*y, objective_font, C_REW_LABEL);
    *y += objective_font + 4.0f;

    int current_step = active ? quest_active_step_index(metadata, progress)
                     : acceptable ? 0 : metadata->step_count;
    for (int i = 0; i < metadata->step_count; i++) {
        bool is_current = i == current_step;
        bool is_done = completed || (active && i < current_step);
        Color step_color = is_current ? C_STAT : is_done ? (Color){ 105, 145, 118, 185 }
                                      : (Color){ 120, 130, 155, 175 };
        float marker_y = *y + text_line_height(step_font) * 0.5f;
        DrawCircle((int)(x + 4.0f), (int)marker_y, is_current ? 4.0f : 3.0f, step_color);

        char line[QUEST_CACHE_STEPDESC_MAX + 24];
        snprintf(line, sizeof(line), "Step %d: %s", i + 1, metadata->steps[i].description);
        *y += text_wrap(line, (int)(x + 14.0f), (int)*y, width - 14,
                        step_font, step_color, false, true);

        const char* live_progress = is_current && progress ? progress->objectives : NULL;
        Color objective_color = is_current ? C_DESC_TEXT : step_color;
        *y += draw_quest_step_objectives(&metadata->steps[i], live_progress,
                                         (int)(x + 22.0f), *y, width - 22,
                                         objective_font, objective_color);
        *y += 4.0f;
    }
}

/* One collapsible mission card in the column at (x, w). Advances *y; records
 * the per-card primary button (Accept / Abandon) so the click handler can act
 * on it. */
static void draw_quest_card(int slot, const char* code, float x, float w,
                            float* y, int mx, int my) {
    quest_cache_fetch(code);
    const QuestProgressEntry* q = quest_progress_store_find(code);
    const QuestMetadataEntry* qm = quest_cache_get(code);

    bool active    = q && QUEST_ACTIVE == q->status;
    bool completed = q && QUEST_COMPLETED == q->status;
    bool locked    = false;
    if (!active && !completed && qm && QUEST_CACHE_READY == qm->state) {
        for (int i = 0; i < qm->prerequisite_count; i++) {
            if (!quest_progress_store_is_completed(qm->prerequisites[i])) { locked = true; break; }
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
    float hx = x, hw = w;
    int qfont = mi_font_quest();
    int rfont = mi_font_rew();
    float chev = mi_q_chevron();
    /* Centralized dynamic-height header: chevron + wrapped title, with the status
     * word reserved on the right so the title never overflows into it. */
    int sww = MeasureText(statusw, rfont);
    float reserve_right = (float)sww + 12.0f;
    float header_h = ui_toggle_header(tg, hx, *y, hw, title, qfont, C_TEXT,
                                      UI_TOGGLE_HEADER_LEFT, 0.0f, reserve_right, chev, false);
    Rectangle header = { hx, *y, hw, header_h };
    s_q_header[slot] = header;
    DrawRectangleRounded(header, 0.18f, 4, (Color){ 30, 34, 52, 205 });
    ui_toggle_header(tg, hx, *y, hw, title, qfont, C_TEXT,
                     UI_TOGGLE_HEADER_LEFT, 0.0f, reserve_right, chev, true);
    DrawText(statusw, (int)(hx + hw - sww - 8),
             (int)(*y + (header_h - text_line_height(rfont)) / 2), rfont, statusc);
    *y += header_h + 4;

    s_q_btn_kind[slot] = 0;
    if (!tg->expanded) { *y += 2; return; }

    float ix = hx + 16;
    int   iw = (int)(hw - 24);

    if (qm && QUEST_CACHE_READY == qm->state) {
        *y += text_wrap(qm->description, (int)ix, (int)*y, iw, mi_font_desc(), C_DESC_TEXT, false, true);
        *y += 4;
        draw_quest_steps(qm, q, active, completed, acceptable, ix, y, iw,
                         mi_font_desc(), rfont);
        if (qm->reward_count > 0) {
            DrawText("Reward:", (int)ix, (int)*y, rfont, C_REW_LABEL);
            float rx = ix + MeasureText("Reward:", rfont) + 8;
            float slot_sz = mi_rew_slot_sz();
            float slot_gap = mi_rew_slot_gap();
            ObjectLayersManager* mgr = obj_layers_mgr_get();
            for (int r = 0; r < qm->reward_count && r < MI_REWARD_SLOT_MAX; r++) {
                ObjectLayerState ol = { 0 };
                strncpy(ol.item_id, qm->rewards[r].item_id, MAX_ID_LENGTH - 1);
                ol.active = true;
                ol.quantity = qm->rewards[r].quantity;
                Rectangle rr = { rx + r * (slot_sz + slot_gap), *y - 4, slot_sz, slot_sz };
                item_slot_draw(rr, &ol, mgr);
                if (s_reward_slot_count < MI_QUEST_MAX * MI_REWARD_SLOT_MAX) {
                    s_reward_rects[s_reward_slot_count] = rr;
                    s_reward_ols[s_reward_slot_count]   = ol;
                    s_reward_slot_count++;
                }
            }
            *y += slot_sz + 2;
        }
    } else {
        if (active && q && q->active_step[0] != '\0') {
            DrawText("Current step", (int)ix, (int)*y, rfont, C_REW_LABEL);
            *y += rfont + 3;
            *y += text_wrap(q->active_step, (int)ix, (int)*y, iw,
                            mi_font_desc(), C_STAT, false, true);
            *y += draw_quest_step_objectives(NULL, q->objectives, (int)ix,
                                             *y, iw, rfont, C_DESC_TEXT);
        } else {
            DrawText("Loading mission details...", (int)ix, (int)*y, mi_font_desc(), C_LABEL);
            *y += mi_font_desc() + 2;
        }
    }

    if (active || acceptable) {
        bool desktop = !viewport_is_mobile();
        float bw = desktop ? MI_Q_BTN_W_DESKTOP : 96.0f;
        float bh = desktop ? MI_Q_BTN_H_DESKTOP : 26.0f;
        int   bfont = desktop ? MI_FONT_QBTN_DESKTOP : (MI_FONT_BTN - 4);
        Rectangle btn = { hx + hw - bw, *y, bw, bh };
        UIButtonStyle st = active
            ? (UIButtonStyle){ .text = "Abandon", .font_size = bfont,
                               .bg = (Color){ 120, 44, 44, 255 }, .text_color = C_TEXT }
            : (UIButtonStyle){ .text = "Accept", .icon_id = "quest", .font_size = bfont,
                               .bg = (Color){ 30, 110, 70, 255 }, .text_color = C_TEXT };
        ui_button_draw(btn, &st, ui_button_resolve_state(true, false, ui_button_hit(btn, mx, my)));
        s_q_btn[slot] = btn;
        s_q_btn_kind[slot] = active ? 2 : 1;
        strncpy(s_q_btn_code[slot], code, 63);
        s_q_btn_code[slot][63] = '\0';
        *y += bh + 4;
    } else if (locked) {
        DrawText("Prerequisite not met.", (int)ix, (int)*y, rfont,
                 (Color){ 210, 120, 110, 220 });
        *y += rfont + 4;
    }
    *y += 4;
}

/* Quest tab: a collapsible card per mission the server authoritatively says
 * this NPC provides to the player (codes from AOI; metadata fetched by code). */
static void draw_quest_tab(Rectangle content, int mx, int my) {
    if (!s_q_toggle_init) {
        for (int i = 0; i < MI_QUEST_MAX; i++)
            ui_toggle_init(&s_q_toggle[i], (Rectangle){ 0, 0, 18, 18 }, 0 == i, UI_TOGGLE_CHEVRON_DOWN);
        s_q_toggle_init = true;
    }
    s_reward_slot_count = 0;
    s_q_count = s_quest_code_count < MI_QUEST_MAX ? s_quest_code_count : MI_QUEST_MAX;

    float content_y = content.y - ui_scroll_offset(&s_q_scroll);
    float y = content_y + 4.0f;
    ui_scroll_begin(&s_q_scroll);
    if (0 == s_q_count) {
        DrawText("No missions available here.", (int)content.x, (int)y, mi_font_quest(), C_LABEL);
        s_q_content_height = y - content_y + mi_font_quest() + 4.0f;
        ui_scroll_end(&s_q_scroll);
        return;
    }
    int lfont = mi_font_qlabel();
    DrawText("Missions", (int)content.x, (int)y, lfont, C_REW_LABEL);
    y += lfont + 6;

    /* Wide (landscape): cards flow into two columns — each card lands in the
     * currently shorter one — instead of a single tall stack. */
    bool  two_col = modal_wide_layout();
    float col_gap = two_col ? 10.0f : 0.0f;
    float col_w   = two_col ? (content.width - col_gap) * 0.5f : content.width;
    float col_x[2] = { content.x, content.x + col_w + col_gap };
    float col_y[2] = { y, y };
    for (int i = 0; i < s_q_count; i++) {
        int c = (two_col && col_y[1] < col_y[0]) ? 1 : 0;
        draw_quest_card(i, s_quest_codes[i], col_x[c], col_w, &col_y[c], mx, my);
    }
    y = col_y[0] > col_y[1] ? col_y[0] : col_y[1];

    s_q_content_height = y - content_y + 4.0f;
    ui_scroll_end(&s_q_scroll);
}

static void handle_quest_click(int mx, int my) {
    for (int i = 0; i < s_q_count; i++) {
        if (0 == s_q_btn_kind[i] || !ui_button_hit(s_q_btn[i], mx, my)) continue;
        if (1 == s_q_btn_kind[i]) {
            local_player_request_quest_accept(s_entity_id, s_q_btn_code[i]);
        } else {
            local_player_request_quest_abandon(s_q_btn_code[i]);
        }
        return;
    }
    for (int i = 0; i < s_reward_slot_count; i++) {
        if (item_slot_hit(s_reward_rects[i], mx, my)) {
            ObjectLayerState ols = s_reward_ols[i];
            es_push();
            modal_interact_close();
            inventory_modal_open_external(&ols);
            inventory_modal_set_on_close(modal_interact_reopen);
            return;
        }
    }
    for (int i = 0; i < s_q_count; i++) {
        if (CheckCollisionPointRec((Vector2){ (float)mx, (float)my }, s_q_header[i])) {
            s_q_toggle[i].expanded = !s_q_toggle[i].expanded;
            return;
        }
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
    DrawRectangle((int)card.x, (int)card.y, (int)card.width, (int)mi_header_h(),
                  (Color){ s_border.r, s_border.g, s_border.b, 40 });
    if (s_display_name[0] != '\0') {
        int name_font = mi_font_name();
        DrawText(s_display_name, (int)(card.x + mi_pad()),
                 (int)(card.y + (mi_header_h() - name_font) * 0.5f), name_font, C_TEXT);
    }
    Rectangle xr = close_rect(card);
    UIButtonStyle close_btn = { .icon_id = "close-yellow", .no_fill = true };
    ui_button_draw(xr, &close_btn, ui_button_resolve_state(true, false, ui_button_hit(xr, mx, my)));

    /* Content panel — faint, mostly transparent so the world shows through;
     * just enough to delimit the active tab's content area. */
    Rectangle content = content_rect(card);
    float pad = mi_pad();
    Rectangle panel = { card.x + pad - 4.0f, card.y + mi_header_h() + mi_tab_h(),
                        card.width - 2.0f * (pad - 4.0f),
                        (card.y + card.height - mi_bar_h()) -
                        (card.y + mi_header_h() + mi_tab_h()) };
    DrawRectangleRec(panel, C_CONTENT);

    /* Tab strip — inactive tabs are transparent with a dimmed icon+label;
     * the active tab gets a clearly visible fill so it reads as selected. Only
     * the capability tabs the entity exposes are drawn. */
    int tabs[MI_TAB_COUNT];
    int tabs_n = visible_tabs(tabs);
    bool tab_shown = false;
    for (int k = 0; k < tabs_n; k++) if (tabs[k] == s_tab) tab_shown = true;
    if (!tab_shown) s_tab = MI_TAB_STACK;
    for (int k = 0; k < tabs_n; k++) {
        int t = tabs[k];
        Rectangle r = tab_rect(card, k);
        UIButtonStyle tb;
        if (t == s_tab) {
            tb = (UIButtonStyle){ .text = MI_TAB_LABEL[t], .icon_id = MI_TAB_ICON[t],
                                  .font_size = mi_font_label(), .bg = C_TAB_ACTIVE,
                                  .text_color = C_TEXT, .rounded = true, .roundness = 0.25f };
        } else {
            tb = (UIButtonStyle){ .text = MI_TAB_LABEL[t], .icon_id = MI_TAB_ICON[t],
                                  .font_size = mi_font_label(), .no_fill = true,
                                  .text_color = C_TAB_DIM };
        }
        ui_button_draw(r, &tb, UI_BUTTON_NORMAL);
    }

    if (s_tab == MI_TAB_STACK)       draw_stack_tab(content);
    else if (s_tab == MI_TAB_STATS)  draw_stats_tab(content);
    else if (s_tab == MI_TAB_QUEST)  draw_quest_tab(content, mx, my);

    /* Fixed bottom bar with integration buttons (right-aligned). No background
     * fill — it shares the modal's panel so the modal's bottom border stays
     * visible and the chrome reads as one surface. */
    Rectangle chat, integration;
    bar_buttons(card, &chat, &integration);

    UIButtonStyle chat_btn = { .text = "Chat", .icon_id = "chat",
                               .font_size = mi_font_btn(), .bg = C_BTN, .text_color = C_TEXT };
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
                                      .font_size = mi_font_btn(), .bg = C_BTN, .text_color = C_TEXT };
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
    int tabs[MI_TAB_COUNT];
    int tabs_n = visible_tabs(tabs);
    for (int k = 0; k < tabs_n; k++) {
        if (ui_button_hit(tab_rect(card, k), mx, my)) {
            s_tab = tabs[k];
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

    if (s_tab == MI_TAB_QUEST &&
        CheckCollisionPointRec((Vector2){ (float)mx, (float)my }, content)) {
        ui_scroll_on_press(&s_q_scroll, mx, my);
        return true;
    }

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
