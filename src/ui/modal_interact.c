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
#include "toolbar.h"
#include "object_layer.h"
#include "object_layers_management.h"
#include "quest_progress_store.h"
#include "quest_cache.h"
#include "ui_button.h"
#include "ui_scroll.h"
#include "ui_icon.h"
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

/* Per-player pending quest-talks (from AOI): one entry per active quest whose
 * current step has an incomplete talk objective this NPC's action maps to a
 * dialogue. Each is offered as its own quest-talk button on the paired
 * dialogue; the greeting stays the default content. */
static char s_talk_quest_codes[BOT_QUEST_CODES_MAX][64] = {{0}};
static char s_talk_dialog_codes[BOT_QUEST_CODES_MAX][64] = {{0}};
static int  s_talk_count = 0;

/* Index of the quest-talk currently shown in the paired dialogue, or -1 while
 * the default greeting is shown. */
static int  s_talk_sel = -1;

/* Authoritative quest codes this NPC provides to the player (from AOI). The
 * Quest tab renders one mission card per code; metadata is fetched by code. */
static char  s_quest_codes[BOT_QUEST_CODES_MAX][64];
static int   s_quest_code_count = 0;

/* True while this modal holds a server interaction context (dlg_start sent on
 * open, freezing the player and binding the entity); released on close. */
static bool  s_dlg_context = false;


/* Max reward icons rendered per mission card. */
#define MI_REWARD_SLOT_MAX 8

/* Multi-mission quest tab: a two-column grid of summary cards. Each card has
 * independent Expand and direct quest-action controls. */
#define MI_QUEST_MAX 8
static Rectangle s_q_grid_btn[MI_QUEST_MAX]; /* grid-mode Expand buttons    */
static Rectangle s_q_grid_header[MI_QUEST_MAX]; /* card header (icon+title+stats) */
static Rectangle s_q_grid_action_btn[MI_QUEST_MAX];
static int       s_q_grid_action_kind[MI_QUEST_MAX]; /* 0 none, 1 accept, 2 abandon */
static int       s_q_expanded = -1;          /* -1 = grid of mission buttons */
static float     s_q_expand_age = MODAL_POP_DURATION;
static Rectangle s_q_close;                  /* detail-mode close button    */
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

/* The paired modal_dialogue opens once the async skin dialogue resolves. */
static bool  s_dialogue_opened = false;
static bool  s_dialogue_open_requested = false;

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
    char  talk_quest_codes[BOT_QUEST_CODES_MAX][64];
    char  talk_dialog_codes[BOT_QUEST_CODES_MAX][64];
    int   talk_count;
    int   talk_sel;
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
    memcpy(f->talk_quest_codes, s_talk_quest_codes, sizeof(s_talk_quest_codes));
    memcpy(f->talk_dialog_codes, s_talk_dialog_codes, sizeof(s_talk_dialog_codes));
    f->talk_count         = s_talk_count;
    f->talk_sel           = s_talk_sel;
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
    memcpy(s_talk_quest_codes, f->talk_quest_codes, sizeof(s_talk_quest_codes));
    memcpy(s_talk_dialog_codes, f->talk_dialog_codes, sizeof(s_talk_dialog_codes));
    s_talk_count         = f->talk_count;
    s_talk_sel           = f->talk_sel;
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
#define MI_Q_GRID_GAP     8.0f
#define MI_Q_GRID_PAD     8.0f
#define MI_Q_GRID_ICON_GAP 6.0f
#define MI_Q_GRID_MIN_H_MOBILE 64.0f
#define MI_Q_GRID_MIN_H_DESKTOP 70.0f
#define MI_FONT_QGRID_MOBILE 15
#define MI_Q_CARD_ACTION_H_MOBILE 28.0f
#define MI_Q_CARD_ACTION_H_DESKTOP 32.0f
#define MI_Q_CARD_ACTION_GAP 6.0f

/* Quest tab desktop enlargement. The mission cards, reward slots, toggle
 * chevrons and Accept/Abandon button read too small at the base sizes above
 * on a full-size monitor; bumped up on desktop only via the mi_* accessors
 * below. Mobile keeps the base sizing untouched (already scaled down by
 * text.c's TEXT_MOBILE_FONT_SCALE). */
#define MI_FONT_QUEST_DESKTOP    17
#define MI_FONT_DESC_DESKTOP     15
#define MI_FONT_REW_DESKTOP      14
#define MI_REW_SLOT_SZ_DESKTOP   44.0f
#define MI_REW_SLOT_GAP_DESKTOP  8.0f
#define MI_Q_BTN_W_DESKTOP       124.0f
#define MI_Q_BTN_H_DESKTOP       34.0f
#define MI_FONT_QBTN_DESKTOP     16

static inline int   mi_font_quest(void)  { return viewport_is_mobile() ? MI_FONT_QUEST : MI_FONT_QUEST_DESKTOP; }
static inline int   mi_font_desc(void)   { return viewport_is_mobile() ? MI_FONT_DESC  : MI_FONT_DESC_DESKTOP; }
static inline int   mi_font_rew(void)    { return viewport_is_mobile() ? MI_FONT_REW   : MI_FONT_REW_DESKTOP; }
static inline float mi_rew_slot_sz(void) { return viewport_is_mobile() ? (float)MI_REW_SLOT_SZ  : MI_REW_SLOT_SZ_DESKTOP; }
static inline float mi_rew_slot_gap(void){ return viewport_is_mobile() ? (float)MI_REW_SLOT_GAP : MI_REW_SLOT_GAP_DESKTOP; }
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

/* Re-read the per-player bot capability snapshot (pending quest-talks, quest
 * codes, interaction flags) from live AOI so the quest-talk buttons and quest
 * tab track server events without a modal reopen. Keeps the last snapshot when
 * the bot has left the AOI. Returns true when the pending quest-talk set
 * changed. The selected quest-talk is re-resolved by quest code, so it survives
 * the set growing or shrinking around it. */
static bool refresh_bot_snapshot(void) {
    const BotState* bot = game_state_find_bot(s_entity_id);
    if (!bot) return false;

    char prev_sel[64] = {0};
    if (0 <= s_talk_sel && s_talk_sel < s_talk_count) {
        strncpy(prev_sel, s_talk_quest_codes[s_talk_sel], sizeof(prev_sel) - 1);
    }
    int  prev_count = s_talk_count;
    char prev_dialogs[BOT_QUEST_CODES_MAX][64];
    memcpy(prev_dialogs, s_talk_dialog_codes, sizeof(prev_dialogs));

    s_interaction_flags = bot->interaction_flags;
    s_quest_code_count = 0;
    s_talk_count = 0;
    for (int i = 0; i < bot->quest_code_count && i < BOT_QUEST_CODES_MAX; i++) {
        strncpy(s_quest_codes[s_quest_code_count], bot->quest_codes[i], 63);
        s_quest_codes[s_quest_code_count][63] = '\0';
        quest_cache_fetch(s_quest_codes[s_quest_code_count]);
        s_quest_code_count++;

        /* Parallel entry: non-empty only for a quest with a pending talk. */
        if ('\0' == bot->quest_talk_dialog_codes[i][0] || s_talk_count >= BOT_QUEST_CODES_MAX) continue;
        strncpy(s_talk_quest_codes[s_talk_count], bot->quest_codes[i], 63);
        s_talk_quest_codes[s_talk_count][63] = '\0';
        strncpy(s_talk_dialog_codes[s_talk_count], bot->quest_talk_dialog_codes[i], 63);
        s_talk_dialog_codes[s_talk_count][63] = '\0';
        s_talk_count++;
    }

    /* Re-point the selection at the same quest; -1 once it is gone. */
    if ('\0' != prev_sel[0]) {
        s_talk_sel = -1;
        for (int i = 0; i < s_talk_count; i++) {
            if (0 == strcmp(s_talk_quest_codes[i], prev_sel)) { s_talk_sel = i; break; }
        }
    } else if (s_talk_sel >= s_talk_count) {
        s_talk_sel = -1;
    }

    if (prev_count != s_talk_count) return true;
    for (int i = 0; i < s_talk_count; i++) {
        if (0 != strcmp(prev_dialogs[i], s_talk_dialog_codes[i])) return true;
    }
    return false;
}

/* Fill `out` with the visible tab IDs in strip order; returns the count. */
/* Quest leads the row when the entity offers missions — it is also the
 * default active tab in that case. */
static int visible_tabs(int out[MI_TAB_COUNT]) {
    int n = 0;
    if (quest_tab_visible())  out[n++] = MI_TAB_QUEST;
    out[n++] = MI_TAB_STACK;
    out[n++] = MI_TAB_STATS;
    return n;
}

/* Desktop uses a top-half card. On mobile, the interact card fills the space
 * otherwise reserved for the paired dialogue. */
#define MI_TOP_FRAC 0.56f

/* Desktop dialogue-collapse expansion: 0 = half-height (paired dialogue
 * below), 1 = the card owns the dialogue's space. Animated in update. */
static float s_dlg_collapse_t = 0.0f;

static Rectangle card_rect(void) {
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();
    float pad = mi_pad();
    float top = toolbar_height() + pad; /* below the top toolbar */
    float hidden_bar_h = inventory_bar_full_height() - inventory_bar_visible_height();
    float max_bottom = sh - inventory_bar_visible_height() -
        (viewport_is_mobile() ? 0.0f : pad);
    float bottom;
    if (viewport_is_mobile()) {
        bottom = max_bottom;
    } else {
        float half = sh * MI_TOP_FRAC - pad * 0.5f + hidden_bar_h;
        if (half > max_bottom) half = max_bottom;
        float e = s_dlg_collapse_t * s_dlg_collapse_t * (3.0f - 2.0f * s_dlg_collapse_t);
        bottom = half + (max_bottom - half) * e;
    }
    if (bottom > max_bottom) bottom = max_bottom;
    if (bottom < top + 120.0f) bottom = top + 120.0f;
    return (Rectangle){ pad, top, (float)sw - 2.0f * pad, bottom - top };
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

/* Mobile adds a Dialog entry alongside the Chat and Integration controls. */
/* Mobile shows the Dialog button while the entity has an active dialogue;
 * desktop shows it only while the paired dialogue is collapsed (its close
 * button handed the space to this modal). The row re-packs otherwise. */
static bool dialog_btn_visible(void) {
    if (viewport_is_mobile()) return s_has_dialogue;
    return modal_dialogue_is_collapsed();
}

static void bar_buttons(Rectangle card, Rectangle* dialog, Rectangle* chat,
                        Rectangle* integration) {
    Rectangle bar = bar_rect(card);
    float pad = mi_pad();
    float button_h = mi_bar_btn_h();
    float by = bar.y + (bar.height - button_h) * 0.5f;
    if (viewport_is_mobile()) {
        int   count = dialog_btn_visible() ? 3 : 2;
        float bw = (bar.width - 2.0f * pad - (float)(count - 1) * MI_BAR_BTN_GAP)
                   / (float)count;
        float x = bar.x + pad;
        *dialog = (Rectangle){ 0 };
        if (dialog_btn_visible()) {
            *dialog = (Rectangle){ x, by, bw, button_h };
            x += bw + MI_BAR_BTN_GAP;
        }
        *chat        = (Rectangle){ x, by, bw, button_h };
        *integration = (Rectangle){ x + bw + MI_BAR_BTN_GAP, by, bw, button_h };
        return;
    }

    *dialog = (Rectangle){ 0 };
    float bw = (bar.width - 3.0f * pad) * 0.5f;
    if (bw > mi_bar_btn_maxw()) bw = mi_bar_btn_maxw();
    float ix = bar.x + bar.width - pad - bw;
    *integration = (Rectangle){ ix, by, bw, button_h };
    *chat        = (Rectangle){ ix - MI_BAR_BTN_GAP - bw, by, bw, button_h };
    if (dialog_btn_visible())
        *dialog = (Rectangle){ chat->x - MI_BAR_BTN_GAP - bw, by, bw, button_h };
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

/* The active dialogue key: the selected quest-talk dialogue code, else the
 * skin greeting. */
static const char* active_dlg_key(void) {
    return 0 <= s_talk_sel && s_talk_sel < s_talk_count
               ? s_talk_dialog_codes[s_talk_sel] : s_dlg_item;
}

/* Open the paired dialogue with the currently selected content: the mapped
 * quest-talk dialogue by full code, or the entity's "default-<skin>"
 * greeting. The server validates a talk objective only when this code matches
 * the action's questDialogueCodes mapping for that quest, so the greeting can
 * never satisfy one. */
static void open_dialogue(const DialogueLine* lines, int count) {
    bool quest_talk = 0 <= s_talk_sel && s_talk_sel < s_talk_count;
    char code[96];
    if (quest_talk)
        snprintf(code, sizeof(code), "%s", s_talk_dialog_codes[s_talk_sel]);
    else
        snprintf(code, sizeof(code), "default-%s", s_dlg_item);
    modal_dialogue_open(s_entity_id, s_dlg_item, code,
                        MODAL_DIALOGUE_RENDER_ENTITY, lines, count);
    /* Yellow quest frame only while a quest-talk dialogue is selected. */
    modal_dialogue_set_quest_style(quest_talk);
    s_dialogue_opened = true;
    if (s_dialogue_open_requested) {
        s_dialogue_open_requested = false;
        modal_dialogue_show_fullscreen();
    }
}

/* Kick off the fetch for the selected dialogue. Render-only when the skin has
 * no greeting and no quest-talk is selected. */
static void request_active_dialogue(void) {
    if (0 <= s_talk_sel && s_talk_sel < s_talk_count)
        dialogue_data_request_code(s_talk_dialog_codes[s_talk_sel]);
    else if (s_has_dialogue)
        dialogue_data_request(s_dlg_item);
    else if (s_talk_count > 0) {
        /* No greeting to fall back to — show the first quest-talk. */
        s_talk_sel = 0;
        dialogue_data_request_code(s_talk_dialog_codes[0]);
    } else {
        open_dialogue(NULL, 0);
    }
}

int modal_interact_quest_talk_count(void) {
    return s_open ? s_talk_count : 0;
}

/* Label for quest-talk button `index`: that quest's live objective, else its
 * current step text, else its title — so each button says what it advances. */
const char* modal_interact_quest_talk_label(int index) {
    if (index < 0 || index >= s_talk_count) return "";
    const char* code = s_talk_quest_codes[index];
    const QuestProgressEntry* q = quest_progress_store_find(code);
    if (q) {
        if ('\0' != q->objectives[0])  return q->objectives;
        if ('\0' != q->active_step[0]) return q->active_step;
        if ('\0' != q->title[0])       return q->title;
    }
    const QuestMetadataEntry* qm = quest_cache_get(code);
    if (qm && '\0' != qm->title[0]) return qm->title;
    return code;
}

/* Index of the quest-talk shown in the paired dialogue, or -1 for the
 * greeting. */
int modal_interact_quest_talk_selected(void) {
    return s_talk_sel;
}

/* Select quest-talk `index` (-1 = default greeting) and reopen the paired
 * dialogue on it. */
void modal_interact_set_quest_talk(int index) {
    if (!s_open || index == s_talk_sel) return;
    if (index >= s_talk_count) return;
    if (index < 0) {
        index = -1;
        if (!s_has_dialogue) return; /* no greeting to fall back to */
    }
    /* Preserve the mobile fullscreen reader across the swap: reopen expanded
     * instead of dropping back to the collapsed footer state. */
    bool was_fullscreen = modal_dialogue_is_fullscreen();
    s_talk_sel = index;
    if (modal_dialogue_is_open()) modal_dialogue_close();
    s_dialogue_opened = false;
    s_dialogue_open_requested = was_fullscreen;
    request_active_dialogue();
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
    s_dialogue_open_requested = false;
    s_overlay_open     = false;
    local_player_request_freeze(true, "interact");

    /* The paired dialogue key was restored by es_pop. Text resolves async in
     * update; mobile renders it only after the footer opens the reader. */
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

/* Drop the pending interact session(s) pushed onto the ephemeral stack so a
 * closed context is not reopened. Used when the inventory modal switches
 * directly to a bar slot instead of returning through the opener chain. */
void modal_interact_discard_stack(void) {
    es_clear();
}

/* ── Public API ───────────────────────────────────────────────────────── */

void modal_interact_init(void) {
    s_open = false;
    s_overlay_open = false;
    s_dialogue_open_requested = false;
    ui_scroll_reset(&s_q_scroll);
    s_q_content_height = 0.0f;
    s_q_expanded = -1;
    s_q_expand_age = MODAL_POP_DURATION;
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
    s_dialogue_open_requested = false;
    s_overlay_open       = false;
    s_open               = true;
    s_tab                = MI_TAB_STACK;
    ui_scroll_reset(&s_q_scroll);
    s_q_content_height = 0.0f;
    s_q_expanded = -1;
    s_q_expand_age = MODAL_POP_DURATION;
    s_dlg_collapse_t = 0.0f;

    /* Only snapshot layers from AOI when we don't already have a cached
     * copy from the ephemeral session.  This way, returning from the
     * inventory inspector reuses the same data we had before. */
    if (s_cached_layer_count == 0) {
        snapshot_alive_layers();
    }

    /* The bot carries, per player, the capability bitmask, the authoritative
     * quest codes it provides, and the pending quest-talk mapped to each. Read
     * these BEFORE requesting the dialogue, since they decide what the paired
     * panel opens on. */
    s_quest_code_count = 0;
    s_talk_count = 0;
    /* The paired dialogue always opens on the default greeting; pending
     * quest-talks are offered through the dialogue's quest buttons instead
     * (request_active_dialogue picks one only when there is no greeting). */
    s_talk_sel = -1;
    refresh_bot_snapshot();

    /* The Quest tab, when offered, opens as the active tab. */
    if (quest_tab_visible()) s_tab = MI_TAB_QUEST;

    /* Interaction freeze for the whole modal session: the server blocks
     * damage/targeting while the reason chain interact→dialogue→interact
     * stays unbroken (dlg_start bridges it; dialogue teardown re-bridges). */
    local_player_request_freeze(true, "interact");

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
    s_dialogue_open_requested = false;
    es_clear();
    if (modal_dialogue_is_open()) modal_dialogue_close();
    local_player_request_freeze(false, "interact");
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
    if (0 <= s_q_expanded && s_q_expand_age < MODAL_POP_DURATION) return true;
    return ui_scroll_on_wheel(&s_q_scroll, content_rect(card_rect()),
                              s_q_content_height, wheel_delta);
}

void modal_interact_update(float dt) {
    if (!s_open) return;
    s_age += dt;

    /* Track the bot's live quest-talk set: a button appears the instant the
     * server posts a pending quest-talk (e.g. right after accepting the
     * mission) and disappears once it resolves — no modal reopen. */
    int prev_sel = s_talk_sel;
    if (refresh_bot_snapshot() && 0 <= prev_sel && s_talk_sel < 0) {
        /* The quest-talk we were showing was consumed server-side: fall back
         * to the greeting (preserving the mobile fullscreen reader). */
        bool was_fullscreen = modal_dialogue_is_fullscreen();
        if (modal_dialogue_is_open()) modal_dialogue_close();
        s_dialogue_opened = false;
        s_dialogue_open_requested = was_fullscreen;
        request_active_dialogue();
    }

    /* Slide between half-height and full-height as the paired dialogue
     * collapses/restores (desktop only; mobile is always full). */
    float target = !viewport_is_mobile() && modal_dialogue_is_collapsed() ? 1.0f : 0.0f;
    float step = dt / 0.25f;
    if (s_dlg_collapse_t < target) {
        s_dlg_collapse_t += step;
        if (s_dlg_collapse_t > target) s_dlg_collapse_t = target;
    } else if (s_dlg_collapse_t > target) {
        s_dlg_collapse_t -= step;
        if (s_dlg_collapse_t < target) s_dlg_collapse_t = target;
    }
    if (0 <= s_q_expanded && s_q_expand_age < MODAL_POP_DURATION) {
        s_q_expand_age += dt;
        if (s_q_expand_age > MODAL_POP_DURATION)
            s_q_expand_age = MODAL_POP_DURATION;
    }

    if (s_tab == MI_TAB_QUEST) {
        ui_scroll_update(&s_q_scroll, content_rect(card_rect()), s_q_content_height, dt);
        int click_x, click_y;
        if (ui_scroll_take_click(&s_q_scroll, &click_x, &click_y) &&
            !modal_notification_is_open()) {
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
    const char* icon_ids[6] = { "stat-effect", "stat-resistance", "stat-agility", "stat-range", "stat-intelligence", "stat-utility" };
    int values[6] = { t.effect, t.resistance, t.agility, t.range, t.intelligence, t.utility };
    float row_h = MI_FONT_STAT + 8.0f;
    float col_w = content.width * 0.5f;
    int icon_sz = MI_FONT_STAT * 2;
    for (int i = 0; i < 6; i++) {
        float cx = content.x + (i % 2) * col_w;
        float cy = content.y + (i / 2) * row_h;
        /* Draw stat icon before the label, matching the inventory modal pattern */
        ui_icon_draw(icon_ids[i], cx + icon_sz / 2.0f, cy + icon_sz / 2.0f, icon_sz, false, 0.0f);
        DrawText(names[i], (int)(cx + icon_sz + 4), (int)cy, MI_FONT_STAT, C_LABEL);
        char val[16];
        snprintf(val, sizeof(val), "%+d", values[i]);
        int vw = MeasureText(val, MI_FONT_STAT);
        DrawText(val, (int)(cx + col_w - vw - mi_pad()), (int)cy, MI_FONT_STAT,
                 values[i] > 0 ? C_STAT : (Color){ 200, 80, 80, 220 });
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

/* Status/title snapshot for one mission code (also kicks the metadata fetch). */
typedef struct {
    const QuestProgressEntry* q;
    const QuestMetadataEntry* qm;
    bool        active, completed, locked, acceptable;
    const char* title;
    const char* word;
    Color       color;
} QuestCardInfo;

static QuestCardInfo quest_card_info(const char* code) {
    quest_cache_fetch(code);
    QuestCardInfo in = { 0 };
    in.q  = quest_progress_store_find(code);
    in.qm = quest_cache_get(code);
    in.active    = in.q && QUEST_ACTIVE == in.q->status;
    in.completed = in.q && QUEST_COMPLETED == in.q->status;
    if (!in.active && !in.completed && in.qm && QUEST_CACHE_READY == in.qm->state) {
        for (int i = 0; i < in.qm->prerequisite_count; i++) {
            if (!quest_progress_store_is_completed(in.qm->prerequisites[i])) {
                in.locked = true;
                break;
            }
        }
    }
    in.acceptable = !in.active && !in.completed && !in.locked;
    in.title = (in.q && in.q->title[0]) ? in.q->title
             : (in.qm && in.qm->title[0]) ? in.qm->title : code;
    in.color = in.active ? (Color){ 220, 190, 70, 255 }
             : in.completed ? (Color){ 110, 210, 130, 235 }
             : in.locked ? (Color){ 210, 120, 110, 230 }
                         : (Color){ 120, 200, 140, 235 };
    in.word = in.active ? "Active" : in.completed ? "Completed"
            : in.locked ? "Locked" : "Available";
    return in;
}

static int quest_grid_font(void) {
    return viewport_is_mobile() ? MI_FONT_QGRID_MOBILE : MI_FONT_QUEST_DESKTOP;
}

static int quest_grid_status_font(int title_font) {
    int status_font = title_font - 3;
    return status_font > 10 ? status_font : 10;
}

static float quest_grid_icon_size(float button_width) {
    return button_width < 140.0f ? 16.0f : 20.0f;
}

static int quest_grid_title_width(float button_width) {
    float title_width = button_width - 2.0f * MI_Q_GRID_PAD -
                        quest_grid_icon_size(button_width) - MI_Q_GRID_ICON_GAP;
    return title_width > 12.0f ? (int)title_width : 12;
}

static void draw_quest_grid_title_line(const char* line, int x, int y,
                                       int width, int font) {
    int line_width = MeasureText(line, font);
    int line_x = x + (width - line_width) / 2;
    for (int offset_y = -1; offset_y <= 1; offset_y++) {
        for (int offset_x = -1; offset_x <= 1; offset_x++) {
            if (0 == offset_x && 0 == offset_y) continue;
            DrawText(line, line_x + offset_x, y + offset_y, font, BLACK);
        }
    }
    DrawText(line, line_x, y, font, WHITE);
}

static void draw_quest_grid_status_line(const char* line, int x, int y,
                                        int width, int font, Color color) {
    int line_width = MeasureText(line, font);
    int line_x = x + (width - line_width) / 2;
    for (int offset_y = -1; offset_y <= 1; offset_y++) {
        for (int offset_x = -1; offset_x <= 1; offset_x++) {
            if (0 == offset_x && 0 == offset_y) continue;
            DrawText(line, line_x + offset_x, y + offset_y, font, BLACK);
        }
    }
    DrawText(line, line_x, y, font, color);
}

static int quest_grid_title_wrap(const char* title, int x, int y, int width,
                                 int font, bool draw) {
    if (NULL == title || '\0' == title[0]) return 0;
    if (width < 1) width = 1;

    char line[QUEST_CACHE_TITLE_MAX] = { 0 };
    char word[QUEST_CACHE_TITLE_MAX] = { 0 };
    char candidate[QUEST_CACHE_TITLE_MAX] = { 0 };
    const char* cursor = title;
    int line_height = text_line_height(font);
    int line_y = y;

    while ('\0' != *cursor) {
        while (' ' == *cursor) cursor++;
        if ('\0' == *cursor) break;

        int word_length = 0;
        while ('\0' != cursor[word_length] && ' ' != cursor[word_length] &&
               word_length < (int)sizeof(word) - 1) {
            word[word_length] = cursor[word_length];
            word_length++;
        }
        word[word_length] = '\0';
        cursor += word_length;

        int word_offset = 0;
        while (word_offset < word_length) {
            if ('\0' != line[0]) {
                snprintf(candidate, sizeof(candidate), "%s %s", line, word + word_offset);
                if (MeasureText(candidate, font) <= width) {
                    strncpy(line, candidate, sizeof(line) - 1);
                    line[sizeof(line) - 1] = '\0';
                    word_offset = word_length;
                    continue;
                }
                if (draw) draw_quest_grid_title_line(line, x, line_y, width, font);
                line_y += line_height;
                line[0] = '\0';
                continue;
            }

            const char* remainder = word + word_offset;
            if (MeasureText(remainder, font) <= width) {
                strncpy(line, remainder, sizeof(line) - 1);
                line[sizeof(line) - 1] = '\0';
                word_offset = word_length;
                continue;
            }

            int fragment_length = 0;
            while (word_offset + fragment_length < word_length) {
                line[fragment_length] = word[word_offset + fragment_length];
                line[fragment_length + 1] = '\0';
                if (MeasureText(line, font) > width && fragment_length > 0) break;
                fragment_length++;
            }
            if (0 == fragment_length) fragment_length = 1;
            line[fragment_length] = '\0';
            word_offset += fragment_length;
            if (word_offset < word_length) {
                if (draw) draw_quest_grid_title_line(line, x, line_y, width, font);
                line_y += line_height;
                line[0] = '\0';
            }
        }
    }

    if ('\0' != line[0]) {
        if (draw) draw_quest_grid_title_line(line, x, line_y, width, font);
        line_y += line_height;
    }
    return line_y - y;
}

static float quest_grid_button_height(const char* title, float button_width,
                                      int font) {
    int title_height = quest_grid_title_wrap(title, 0, 0,
                                             quest_grid_title_width(button_width),
                                             font, false);
    int status_height = text_line_height(quest_grid_status_font(font));
    float icon_size = quest_grid_icon_size(button_width);
    float text_height = title_height + 2.0f + status_height;
    float content_height = text_height > icon_size ? text_height : icon_size;
    float min_height = viewport_is_mobile() ? MI_Q_GRID_MIN_H_MOBILE
                                             : MI_Q_GRID_MIN_H_DESKTOP;
    float action_height = viewport_is_mobile() ? MI_Q_CARD_ACTION_H_MOBILE
                                                : MI_Q_CARD_ACTION_H_DESKTOP;
    float button_height = content_height + 2.0f * MI_Q_GRID_PAD +
                          MI_Q_CARD_ACTION_GAP + action_height;
    return button_height > min_height ? button_height : min_height;
}

static int quest_grid_action_font(const char* label, float button_width) {
    int font = viewport_is_mobile() ? 12 : 14;
    while (font > 9 && MeasureText(label, font) > (int)button_width - 8) font--;
    return font;
}

static void draw_quest_grid_action_button(Rectangle button, const char* label,
                                          Color color, bool enabled, int mx, int my) {
    bool hovered = enabled && ui_button_hit(button, mx, my);
    Color fill = enabled ? color : (Color){ 58, 62, 76, 255 };
    Color light = hovered ? (Color){ 255, 255, 220, 255 } : (Color){ 220, 225, 235, 190 };
    Color shade = enabled ? (Color){ 10, 14, 24, 255 } : (Color){ 28, 30, 38, 255 };
    Rectangle inner = { button.x + 2.0f, button.y + 2.0f,
                        button.width - 4.0f, button.height - 4.0f };

    DrawRectangleRec(button, BLACK);
    DrawRectangleRec(inner, fill);
    DrawRectangle((int)inner.x, (int)inner.y, (int)inner.width, 2, light);
    DrawRectangle((int)inner.x, (int)(inner.y + inner.height - 2.0f),
                  (int)inner.width, 2, shade);
    if (hovered) DrawRectangleLinesEx(inner, 1.0f, WHITE);

    int font = quest_grid_action_font(label, button.width);
    int label_y = (int)(button.y + (button.height - text_line_height(font)) * 0.5f);
    draw_quest_grid_title_line(label, (int)button.x, label_y, (int)button.width, font);
}

static void draw_quest_grid_button(Rectangle card, const QuestCardInfo* info,
                                   int slot, int font, int mx, int my) {
    bool hovered = ui_button_hit(card, mx, my);
    Color fill = hovered ? (Color){ 35, 48, 72, 255 } : (Color){ 24, 32, 50, 255 };
    Color highlight = hovered ? (Color){ 86, 112, 152, 255 } : (Color){ 58, 78, 110, 255 };
    Color shadow = (Color){ 8, 12, 22, 255 };
    Rectangle inner = { card.x + 2.0f, card.y + 2.0f,
                        card.width - 4.0f, card.height - 4.0f };
    float action_height = viewport_is_mobile() ? MI_Q_CARD_ACTION_H_MOBILE
                                                : MI_Q_CARD_ACTION_H_DESKTOP;
    float action_y = card.y + card.height - MI_Q_GRID_PAD - action_height;
    float action_width = (card.width - 2.0f * MI_Q_GRID_PAD - MI_Q_CARD_ACTION_GAP) * 0.5f;

    DrawRectangleRec(card, BLACK);
    DrawRectangleRec(inner, fill);
    DrawRectangle((int)inner.x, (int)inner.y, (int)inner.width, 2, highlight);
    DrawRectangle((int)inner.x, (int)(inner.y + inner.height - 2.0f),
                  (int)inner.width, 2, shadow);
    DrawRectangle((int)inner.x, (int)(action_y - MI_Q_CARD_ACTION_GAP * 0.5f),
                  (int)inner.width, 1, info->color);
    DrawRectangle((int)inner.x, (int)inner.y, 3, (int)inner.height, info->color);
    if (hovered) DrawRectangleLinesEx(inner, 1.0f, WHITE);

    float icon_size = quest_grid_icon_size(card.width);
    float icon_x = card.x + MI_Q_GRID_PAD + icon_size * 0.5f;
    float content_top = card.y + MI_Q_GRID_PAD;
    float content_bottom = action_y - MI_Q_CARD_ACTION_GAP;
    float icon_y = content_top + (content_bottom - content_top) * 0.5f;
    ui_icon_draw_ex("quest", icon_x + 1.0f, icon_y + 1.0f, icon_size, 0.0f, BLACK);
    ui_icon_draw_ex("quest", icon_x, icon_y, icon_size, 0.0f, WHITE);

    int title_x = (int)(card.x + MI_Q_GRID_PAD + icon_size + MI_Q_GRID_ICON_GAP);
    int title_width = quest_grid_title_width(card.width);
    int title_height = quest_grid_title_wrap(info->title, title_x, 0,
                                              title_width, font, false);
    int status_font = quest_grid_status_font(font);
    int status_height = text_line_height(status_font);
    int text_height = title_height + 2 + status_height;
    int title_y = (int)(content_top + (content_bottom - content_top - text_height) * 0.5f);
    quest_grid_title_wrap(info->title, title_x, title_y, title_width, font, true);
    draw_quest_grid_status_line(info->word, title_x, title_y + title_height + 2,
                                title_width, status_font, info->color);

    /* The header area (icon + title + stats, above the action row) also
     * expands the card when tapped. */
    s_q_grid_header[slot] = (Rectangle){ card.x, card.y, card.width,
                                         action_y - MI_Q_CARD_ACTION_GAP - card.y };
    s_q_grid_btn[slot] = (Rectangle){ card.x + MI_Q_GRID_PAD, action_y,
                                       action_width, action_height };
    const char* action_label = info->acceptable ? "Accept"
                             : info->active ? "Abandon" : info->word;
    Color action_color = info->acceptable ? (Color){ 38, 138, 76, 255 }
                       : info->active ? (Color){ 150, 48, 52, 255 }
                       : (Color){ 70, 74, 88, 255 };
    s_q_grid_action_kind[slot] = info->acceptable ? 1 : info->active ? 2 : 0;
    s_q_grid_action_btn[slot] = (Rectangle){ s_q_grid_btn[slot].x + action_width +
                                              MI_Q_CARD_ACTION_GAP, action_y,
                                              action_width, action_height };
    draw_quest_grid_action_button(s_q_grid_btn[slot], "Expand",
                                  (Color){ 44, 96, 156, 255 }, true, mx, my);
    draw_quest_grid_action_button(s_q_grid_action_btn[slot], action_label, action_color,
                                  0 != s_q_grid_action_kind[slot], mx, my);
}

/* Expanded mission detail at (x, w): title row, then its primary action,
 * description, steps, and rewards. Advances *y. */
static void draw_quest_detail(int slot, const char* code, float x, float w,
                              float* y, int mx, int my) {
    QuestCardInfo in = quest_card_info(code);
    const QuestProgressEntry* q = in.q;
    const QuestMetadataEntry* qm = in.qm;
    bool active = in.active, completed = in.completed;
    bool locked = in.locked, acceptable = in.acceptable;
    int qfont = mi_font_quest();
    int rfont = mi_font_rew();

    /* Title row. */
    float close_sz = 26.0f;
    float row_top  = *y;
    s_q_close = (Rectangle){ x + w - close_sz - 2.0f, row_top, close_sz, close_sz };
    UIButtonStyle cb = { .icon_id = "close-yellow", .no_fill = true };
    ui_button_draw(s_q_close, &cb,
                   ui_button_resolve_state(true, false, ui_button_hit(s_q_close, mx, my)));

    ui_icon_draw_ex("quest", x + 11.0f, row_top + 13.0f, 20.0f, 0.0f, WHITE);
    int sww = MeasureText(in.word, rfont);
    DrawText(in.word, (int)(x + w - close_sz - sww - 12.0f),
             (int)(row_top + (close_sz - text_line_height(rfont)) * 0.5f), rfont, in.color);
    float th = (float)text_wrap(in.title, (int)(x + 26.0f), (int)(row_top + 3.0f),
                                (int)(w - close_sz - sww - 26.0f - 24.0f),
                                qfont, C_TEXT, false, true);
    float row_h = th + 6.0f > close_sz + 4.0f ? th + 6.0f : close_sz + 4.0f;
    *y += row_h + 4.0f;

    s_q_btn_kind[slot] = 0;

    float ix = x + 16;
    int   iw = (int)(w - 24);

    if (active || acceptable) {
        bool desktop = !viewport_is_mobile();
        int bfont = desktop ? MI_FONT_QBTN_DESKTOP : (MI_FONT_BTN - 3);
        const char* label = active ? "Abandon quest" : "Accept quest";
        UIButtonPixelRetroStyle st = {
            .bg = active ? (Color){ 150, 46, 46, 255 } : (Color){ 38, 138, 76, 255 },
            .icon_id = active ? "close" : "arrow-right",
            .label = label, .font_size = bfont, .enabled = true,
        };
        /* Content-fit width: icon + gap + label + padding. */
        float button_width = (float)MeasureText(label, bfont) + bfont + 6.0f + 28.0f;
        if (button_width > (float)iw) button_width = (float)iw;
        float button_height = desktop ? MI_Q_BTN_H_DESKTOP : 32.0f;
        Rectangle btn = { ix, *y, button_width, button_height };
        ui_button_pixel_retro_draw(btn, &st, ui_button_hit(btn, mx, my));
        s_q_btn[slot] = btn;
        s_q_btn_kind[slot] = active ? 2 : 1;
        strncpy(s_q_btn_code[slot], code, 63);
        s_q_btn_code[slot][63] = '\0';
        *y += button_height + 8.0f;
    }

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

    if (locked) {
        DrawText("Prerequisite not met.", (int)ix, (int)*y, rfont,
                 (Color){ 210, 120, 110, 220 });
        *y += rfont + 4;
    }
    *y += 4;
}

/* Quest tab: mission codes the server authoritatively says this NPC provides
 * to the player (from AOI; metadata fetched by code). Grid mode shows one
 * content-fit card button per mission; tapping one switches to that
 * mission's full detail. */
static void draw_quest_tab(Rectangle content, int mx, int my) {
    s_reward_slot_count = 0;
    s_q_count = s_quest_code_count < MI_QUEST_MAX ? s_quest_code_count : MI_QUEST_MAX;
    if (s_q_expanded >= s_q_count) {
        s_q_expanded = -1;
        s_q_expand_age = MODAL_POP_DURATION;
    }

    Rectangle draw_content = content;
    if (0 <= s_q_expanded)
        draw_content = modal_scale_rect(content, modal_pop_scale(s_q_expand_age));
    float content_y = draw_content.y - ui_scroll_offset(&s_q_scroll);
    float y = content_y + 4.0f;
    ui_scroll_begin(&s_q_scroll);
    if (0 == s_q_count) {
        DrawText("No missions available here.", (int)content.x, (int)y, mi_font_quest(), C_LABEL);
        s_q_content_height = y - content_y + mi_font_quest() + 4.0f;
        ui_scroll_end(&s_q_scroll);
        return;
    }

    if (0 <= s_q_expanded) {
        draw_quest_detail(s_q_expanded, s_quest_codes[s_q_expanded],
                          draw_content.x, draw_content.width, &y, mx, my);
    } else {
        int qfont = quest_grid_font();
        float column_width = (content.width - MI_Q_GRID_GAP) * 0.5f;
        for (int first_slot = 0; first_slot < s_q_count; first_slot += 2) {
            QuestCardInfo left = quest_card_info(s_quest_codes[first_slot]);
            float row_height = quest_grid_button_height(left.title, column_width, qfont);
            QuestCardInfo right = { 0 };
            bool has_right = first_slot + 1 < s_q_count;
            if (has_right) {
                right = quest_card_info(s_quest_codes[first_slot + 1]);
                float right_height = quest_grid_button_height(right.title, column_width, qfont);
                if (right_height > row_height) row_height = right_height;
            }

            Rectangle left_button = { content.x, y, column_width, row_height };
            draw_quest_grid_button(left_button, &left, first_slot, qfont, mx, my);
            if (has_right) {
                Rectangle right_button = { content.x + column_width + MI_Q_GRID_GAP,
                                           y, column_width, row_height };
                draw_quest_grid_button(right_button, &right, first_slot + 1,
                                       qfont, mx, my);
            }

            y += row_height;
            if (first_slot + 2 < s_q_count) y += MI_Q_GRID_GAP;
        }
        y += 4.0f;
    }

    s_q_content_height = y - content_y + 4.0f;
    ui_scroll_end(&s_q_scroll);
}

static void request_quest_action(int kind, const char* code) {
    if (1 == kind) {
        local_player_request_quest_accept(s_entity_id, code);
    } else if (2 == kind) {
        local_player_request_quest_abandon(code);
    }
}

static void handle_quest_click(int mx, int my) {
    if (0 <= s_q_expanded) {
        if (s_q_expand_age < MODAL_POP_DURATION) return;
        /* Detail mode: close returns to the grid; then the mission's
         * Accept/Abandon button and reward slots. */
        if (ui_button_hit(s_q_close, mx, my)) {
            s_q_expanded = -1;
            s_q_expand_age = MODAL_POP_DURATION;
            ui_scroll_reset(&s_q_scroll);
            return;
        }
        int i = s_q_expanded;
        if (0 != s_q_btn_kind[i] && ui_button_hit(s_q_btn[i], mx, my)) {
            request_quest_action(s_q_btn_kind[i], s_q_btn_code[i]);
            return;
        }
        for (int r = 0; r < s_reward_slot_count; r++) {
            if (item_slot_hit(s_reward_rects[r], mx, my)) {
                ObjectLayerState ols = s_reward_ols[r];
                es_push();
                modal_interact_close();
                inventory_modal_open_external(&ols);
                inventory_modal_set_on_close(modal_interact_reopen);
                return;
            }
        }
        return;
    }

    /* Grid mode: direct action controls precede their Expand targets. */
    for (int i = 0; i < s_q_count; i++) {
        if (0 != s_q_grid_action_kind[i] &&
            ui_button_hit(s_q_grid_action_btn[i], mx, my)) {
            request_quest_action(s_q_grid_action_kind[i], s_quest_codes[i]);
            return;
        }
        /* Expand button or a tap anywhere on the card header expands it. */
        if (ui_button_hit(s_q_grid_btn[i], mx, my) ||
            ui_button_hit(s_q_grid_header[i], mx, my)) {
            s_q_expanded = i;
            s_q_expand_age = 0.0f;
            ui_scroll_reset(&s_q_scroll);
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
    Rectangle xr = close_rect(card);
    if (s_display_name[0] != '\0') {
        int name_font = mi_font_name();
        /* Clears the toolbar's pinned top-left toggle when the strip hides. */
        DrawText(s_display_name, (int)toolbar_toggle_right(),
                 (int)(card.y + (mi_header_h() - name_font) * 0.5f), name_font, C_TEXT);
    }
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
     * the capability tabs the entity exposes are drawn. Pixel retro style. */
    int tabs[MI_TAB_COUNT];
    int tabs_n = visible_tabs(tabs);
    bool tab_shown = false;
    for (int k = 0; k < tabs_n; k++) if (tabs[k] == s_tab) tab_shown = true;
    if (!tab_shown) s_tab = MI_TAB_STACK;
    for (int k = 0; k < tabs_n; k++) {
        int t = tabs[k];
        Rectangle r = tab_rect(card, k);
        bool hovered = ui_button_hit(r, mx, my);
        if (t == s_tab) {
            UIButtonPixelRetroStyle st = {
                .bg = C_TAB_ACTIVE,
                .icon_id = MI_TAB_ICON[t],
                .label = MI_TAB_LABEL[t],
                .font_size = mi_font_label(),
                .text_color = C_TEXT,
                .selected = true,
                .enabled = true,
            };
            ui_button_pixel_retro_draw(r, &st, hovered);
        } else {
            UIButtonPixelRetroStyle st = {
                .bg = (Color){ 24, 30, 48, 255 },
                .icon_id = MI_TAB_ICON[t],
                .label = MI_TAB_LABEL[t],
                .font_size = mi_font_label(),
                .text_color = C_TAB_DIM,
                .selected = false,
                .enabled = true,
            };
            ui_button_pixel_retro_draw(r, &st, hovered);
        }
    }

    if (s_tab == MI_TAB_STACK)       draw_stack_tab(content);
    else if (s_tab == MI_TAB_STATS)  draw_stats_tab(content);
    else if (s_tab == MI_TAB_QUEST)  draw_quest_tab(content, mx, my);

    /* Fixed bottom bar with integration buttons. No background
     * fill — it shares the modal's panel so the modal's bottom border stays
     * visible and the chrome reads as one surface. */
    Rectangle dialog, chat, integration;
    bar_buttons(card, &dialog, &chat, &integration);

    if (dialog_btn_visible()) {
        /* A pending quest-talk marks the Dialog button: quest icon + yellow
         * border so the mission entry stands out among Chat / Integration. */
        bool pending_quest_talk = modal_interact_quest_talk_count() > 0;
        int dfont = viewport_is_mobile() ? 12 : mi_font_btn();
        UIButtonPixelRetroStyle dialog_st = {
            .bg = C_BTN,
            .icon_id = pending_quest_talk ? "quest" : "chat",
            .label = "Dialog",
            .font_size = dfont,
            .text_color = C_TEXT,
            .selected = false,
            .enabled = true,
        };
        ui_button_pixel_retro_draw(dialog, &dialog_st, ui_button_hit(dialog, mx, my));
        /* Yellow border overlay for quest-talk active state. */
        if (pending_quest_talk)
            DrawRectangleRoundedLinesEx(dialog, 0.18f, 6, 2.0f, (Color){ 230, 200, 60, 230 });
    }

    int cfont = viewport_is_mobile() ? 12 : mi_font_btn();
    UIButtonPixelRetroStyle chat_st = {
        .bg = C_BTN,
        .icon_id = "chat",
        .label = "Chat",
        .font_size = cfont,
        .text_color = C_TEXT,
        .selected = false,
        .enabled = true,
    };
    ui_button_pixel_retro_draw(chat, &chat_st, ui_button_hit(chat, mx, my));

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

    int ifont = viewport_is_mobile() ? 12 : mi_font_btn();
    UIButtonPixelRetroStyle integration_st = {
        .bg = C_BTN,
        .icon_id = "reload",
        .label = "Integration",
        .font_size = ifont,
        .text_color = C_TEXT,
        .selected = false,
        .enabled = true,
    };
    ui_button_pixel_retro_draw(integration, &integration_st, ui_button_hit(integration, mx, my));
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

    /* Tab strip — clicking the already-active quest tab while a quest is
     * expanded returns to the grid view. */
    int tabs[MI_TAB_COUNT];
    int tabs_n = visible_tabs(tabs);
    for (int k = 0; k < tabs_n; k++) {
        if (ui_button_hit(tab_rect(card, k), mx, my)) {
            int new_tab = tabs[k];
            if (new_tab == MI_TAB_QUEST && new_tab == s_tab && s_q_expanded >= 0) {
                s_q_expanded = -1;
                s_q_expand_age = MODAL_POP_DURATION;
                ui_scroll_reset(&s_q_scroll);
            }
            s_tab = new_tab;
            return true;
        }
    }

    /* Bottom bar integration buttons */
    Rectangle dialog, chat, integration;
    bar_buttons(card, &dialog, &chat, &integration);
    if (dialog_btn_visible() && ui_button_hit(dialog, mx, my)) {
        if (s_dialogue_opened)
            modal_dialogue_show_fullscreen();
        else
            s_dialogue_open_requested = true;
        return true;
    }
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
        if (0 <= s_q_expanded && s_q_expand_age < MODAL_POP_DURATION) return true;
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
