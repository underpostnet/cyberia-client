#include "quest_journal.h"
#include "text.h"

#include "hud_side_stack.h"
#include "modal.h"
#include "modal_instance_map.h"
#include "data/instance_map_data.h"
#include "data/quest_cache.h"
#include "data/quest_progress_store.h"
#include "data/quest_target.h"
#include "ui_button.h"
#include "ui_icon.h"
#include "ui_scroll.h"
#include "ui_toggle.h"

#include <raylib.h>
#include <stdio.h>
#include <string.h>

/* ── Layout ───────────────────────────────────────────────────────────── */

#define QJ_HEADER_H       28
#define QJ_HEADER_PAD     7
#define QJ_PAGER_H        22
#define QJ_CHEVRON        32

#define QJ_FONT_TITLE     11
#define QJ_FONT_SECTION   14
#define QJ_FONT_NAME      13
#define QJ_FONT_SMALL     11

#define QJ_LOCATE_W       92   /* the LOCATE button of an objective       */
#define QJ_LOCATE_H       30
#define QJ_LOCATE_BAND    44   /* its tap band: an objective is at least this tall */

#define QJ_CARD_MX        6    /* card horizontal margin inside the panel */
#define QJ_CARD_PAD       7    /* card inner padding                      */
#define QJ_CARD_GAP       6    /* vertical gap between cards              */

/* Header and cards sit over the translucent HUD panel. */
static const Color C_HEADER   = {   3,   7,  16, 148 };
static const Color C_HEADER_TEXT = { 215, 230, 245, 235 };
static const Color C_DIM      = { 140, 140, 160, 200 };
static const Color C_CARD     = {  30,  34,  52, 210 };
static const Color C_STEP     = { 120, 200, 140, 235 };
static const Color C_STEP_DONE = { 105, 145, 118, 185 }; /* muted green — completed step */
static const Color C_DIS      = {  90,  90, 110, 160 };  /* also: future (locked) steps  */
static const Color C_HELD     = { 230, 170, 110, 235 };  /* rewards wait for a bag slot  */

static const Color C_STATUS[QUEST_STATUS_COUNT] = {
    { 220, 190,  60, 255 }, /* active    — gold */
    {  90, 200, 110, 255 }, /* completed — green */
    { 210,  80,  80, 255 }, /* failed    — red */
};

static const char* C_SECTION_LABEL[QUEST_STATUS_COUNT] = {
    "Active Quests", "Completed Quests", "Failed Quests",
};

/* ── State ────────────────────────────────────────────────────────────── */

enum { JW_DRAW, JW_CLICK, JW_MEASURE };

static bool     s_init      = false;
static bool     s_visible   = false; /* toolbar quest button toggles */
static float    s_age       = 0.0f;
static float    s_panel_h   = QJ_HEADER_H;
static float    s_header_h  = QJ_HEADER_H; /* fixed header height (never scrolls) */
static float    s_content_h = 0.0f;  /* full section height below the header      */
static Rectangle s_view     = { 0 };  /* scroll viewport captured for update       */
static UIScroll s_scroll;             /* clips + scrolls the section content       */
static UIToggle s_section[QUEST_STATUS_COUNT];
static int      s_page[QUEST_STATUS_COUNT]   = { 0, 0, 0 };

/* Shows or hides the journal. While it shows it holds the world map: where its objectives lead. */
static void set_visible(bool visible) {
    if (visible == s_visible) return;
    s_visible = visible;
    if (visible) instance_map_data_open();
    else instance_map_data_close();
}

static int page_count(int count) {
    if (count <= 0) return 1;
    return (count + QUEST_JOURNAL_PAGE_SIZE - 1) / QUEST_JOURNAL_PAGE_SIZE;
}

/* The journal's slot in the right-hand column: under the minimap when that
 * is open, as tall as the content needs within the room the column leaves. */
static Rectangle panel_rect(void) {
    return hud_side_stack_journal_slot(s_header_h + s_content_h);
}

static void ensure_init(void) {
    if (s_init) return;
    Rectangle z = { 0, 0, QJ_CHEVRON, QJ_CHEVRON };
    for (int i = 0; i < QUEST_STATUS_COUNT; ++i) {
        ui_toggle_init(&s_section[i], z, i == QUEST_ACTIVE, UI_TOGGLE_CHEVRON_DOWN);
    }
    ui_scroll_reset(&s_scroll);
    s_init = true;
}

/* Where an objective leads in this world, in one line. */
static void destination_text(const QuestTarget* target, char* out, size_t size) {
    switch (target->state) {
        case QUEST_TARGET_PENDING:   snprintf(out, size, "Loading the world map..."); return;
        case QUEST_TARGET_ABSENT:    snprintf(out, size, "Not in this world"); return;
        case QUEST_TARGET_NO_SOURCE: snprintf(out, size, "No known source in this world"); return;
        case QUEST_TARGET_FOUND:
        case QUEST_TARGET_UNREACHABLE: break;
    }
    const ImapGraph* gr = instance_map_data_graph();
    const ImapPresencePoi* poi = &gr->presence_pois[target->destinations[0].poi];
    char map[IMAP_NAME_MAX * 2];
    instance_map_node_label(gr, poi->node, map, sizeof map);
    const char* route = QUEST_TARGET_UNREACHABLE == target->state ? " - no route from here" : "";
    if (1 == target->destination_count)
        snprintf(out, size, "%s (%d, %d)%s", map, poi->cell_x, poi->cell_y, route);
    else
        snprintf(out, size, "%d places - nearest %s (%d, %d)%s", target->destination_count, map, poi->cell_x,
                 poi->cell_y, route);
}

/* The objectives of the step a quest is on: the count the server keeps, then
 * where each one leads. An objective that leads somewhere has a LOCATE button,
 * which follows it and shows its nearest destination on the map. The followed
 * one reads FOLLOWING and shows the destination in focus again. The whole
 * height of the objective, at least QJ_LOCATE_BAND, takes the tap. */
static int objectives_layout(int mode, int mx, int my, const QuestProgressEntry* e, int x, int y, int w) {
    int cy = y;
    const QuestTarget* followed = quest_target_tracked();
    for (int i = 0; e->objective_count > i; i++) {
        const QuestObjectiveProgress* o = &e->objectives[i];
        bool done = o->current >= o->required;
        const QuestTarget* target = done ? NULL : quest_target_find(e->code, e->step_id, o->type, o->item_id);
        bool locate = NULL != target && 0 < target->destination_count;
        int text_w = locate ? w - QJ_LOCATE_W - 6 : w, top = cy;
        char line[MAX_ITEM_ID_LENGTH + 48];
        snprintf(line, sizeof line, "%s %s %d/%d%s", quest_objective_verb(o->type), o->item_id, o->current,
                 o->required, done ? " - done" : "");
        cy += text_wrap(line, x, cy, text_w, QJ_FONT_SMALL, done ? C_STEP_DONE : MODAL_TEXT, false, JW_DRAW == mode);
        if (NULL != target) {
            char where[IMAP_NAME_MAX * 2 + 48];
            destination_text(target, where, sizeof where);
            cy += text_wrap(where, x + 8, cy, text_w - 8, QJ_FONT_SMALL, C_DIM, false, JW_DRAW == mode);
        }
        if (!locate) continue;
        if (QJ_LOCATE_BAND > cy - top) cy = top + QJ_LOCATE_BAND;
        bool on = target == followed;
        Rectangle band = { (float)(x + w - QJ_LOCATE_W), (float)top, QJ_LOCATE_W, (float)(cy - top) };
        Rectangle button = { band.x, band.y + (band.height - QJ_LOCATE_H) * 0.5f, QJ_LOCATE_W, QJ_LOCATE_H };
        if (JW_DRAW == mode) {
            UIButtonPixelRetroStyle style = { .bg = on ? MODAL_SELECTED : MODAL_BUTTON, .icon_id = "target",
                                              .label = on ? "FOLLOWING" : "LOCATE", .font_size = 10,
                                              .text_color = on ? WHITE : MODAL_QUEST, .selected = on, .enabled = true };
            ui_button_pixel_retro_draw(button, &style, ui_button_hit(band, GetMouseX(), GetMouseY()));
        } else if (JW_CLICK == mode && ui_button_hit(band, mx, my)) {
            quest_target_track(target, on ? quest_target_tracked_destination() : 0);
            modal_instance_map_locate();
        }
    }
    return cy - y;
}

/* Render, measure or hit-test one quest as a single flat card — no nested
 * expand: a status dot, the prominent quest name, then for an active quest its
 * code and world, its steps and the objectives of the current one; else a
 * one-word status (Completed / Abandoned). Everything wraps to the card width.
 * Returns the card's pixel height. */
static int quest_card_layout(int mode, int mx, int my, const QuestProgressEntry* e, QuestStatus sec, int x, int y,
                             int w) {
    bool draw = JW_DRAW == mode;
    const char* name = e->title[0] != '\0' ? e->title : e->code;
    int tx = x + QJ_CARD_PAD + 12;                 /* text column, right of the dot */
    int tw = w - (QJ_CARD_PAD + 12) - QJ_CARD_PAD;
    int cy = y + QJ_CARD_PAD;

    if (draw) {
        DrawCircle(x + QJ_CARD_PAD + 4, cy + QJ_FONT_NAME / 2, 4.0f, C_STATUS[sec]);
    }
    cy += text_wrap(name, tx, cy, tw, QJ_FONT_NAME, MODAL_TEXT, false, draw);
    cy += 2;

    if (QUEST_ACTIVE == sec) {
        char sub[META_CACHE_CODE_MAX + IMAP_NAME_MAX + 4];
        bool world = IMAP_DATA_READY == instance_map_data_state();
        snprintf(sub, sizeof sub, "%s%s%s", e->code, world ? " - " : "", world ? instance_map_data_graph()->name : "");
        cy += text_wrap(sub, tx, cy, tw, QJ_FONT_SMALL, C_DIM, false, draw);
        /* Full step list when metadata is cached: completed steps in muted
         * green, the current step as the green focus (with its objectives),
         * future steps visually disabled. */
        const QuestMetadataEntry* qm = quest_cache_get(e->code);
        if (qm && META_CACHE_READY == qm->head.state && qm->step_count > 0) {
            int active_idx = quest_active_step_index(qm, e);
            for (int s = 0; s < qm->step_count; s++) {
                bool current = s == active_idx;
                Color c = current ? C_STEP : (s < active_idx ? C_STEP_DONE : C_DIS);
                char line[QUEST_STEP_MAX + 4];
                snprintf(line, sizeof(line), "%s %s", current ? ">" : "-", qm->steps[s].description);
                cy += text_wrap(line, tx, cy, tw, QJ_FONT_SMALL, c, false, draw);
                if (current) cy += objectives_layout(mode, mx, my, e, tx + 8, cy, tw - 8);
            }
        } else {
            cy += text_wrap(e->active_step, tx, cy, tw, QJ_FONT_SMALL, C_STEP, false, draw);
            cy += objectives_layout(mode, mx, my, e, tx, cy, tw);
        }
        if (e->rewards_held) {
            cy += text_wrap("Rewards wait: free a bag slot.", tx, cy, tw, QJ_FONT_SMALL, C_HELD, false, draw);
        }
    } else {
        const char* status = (QUEST_COMPLETED == sec) ? "Completed" : "Abandoned";
        cy += text_wrap(status, tx, cy, tw, QJ_FONT_SMALL, C_STATUS[sec], false, draw);
    }
    return (cy - y) + QJ_CARD_PAD;
}

/* ── Layout walks (single source of truth) ────────────────────────────── */
/* JW_DRAW renders, JW_CLICK hit-tests + applies actions, JW_MEASURE only
 * advances the y cursor to size the region. The header is fixed; the sections
 * scroll inside the clipped viewport. */

/* Fixed title bar (no collapse) with a close-yellow button that hides the
 * journal. The panel is always open; only the close button acts. */
static float header_walk(int mode, int mx, int my, float x, float y, float w) {
    float header_h = QJ_HEADER_H;
    float close_sz = 24.0f;
    Rectangle close_r = { x + w - close_sz - 4.0f, y + (header_h - close_sz) * 0.5f,
                          close_sz, close_sz };
    if (JW_DRAW == mode) {
        Rectangle header = { x, y, w, header_h };
        DrawRectangleRec(header, C_HEADER);
        DrawText("QUEST JOURNAL", (int)(x + QJ_HEADER_PAD),
                 (int)(y + (header_h - QJ_FONT_TITLE) * 0.5f),
                 QJ_FONT_TITLE, C_HEADER_TEXT);
        ui_button_draw_close(close_r);
    } else if (JW_CLICK == mode && ui_button_hit(close_r, mx, my)) {
        set_visible(false);
    }
    return header_h;
}

/* Walk the three status sections starting at y0 (already shifted by the scroll
 * offset in draw/click). Returns the bottom y so callers can size content. */
static float sections_walk(int mode, int mx, int my, float x, float y0, float w) {
    float y = y0;
    for (int sec = 0; sec < QUEST_STATUS_COUNT; ++sec) {
        int count = quest_progress_store_count((QuestStatus)sec);

        char label[64];
        snprintf(label, sizeof(label), "%s (%d)", C_SECTION_LABEL[sec], count);
        float srow_h = ui_toggle_header(&s_section[sec], x, y, w, label, QJ_FONT_SECTION,
                                        MODAL_TEXT, JW_DRAW == mode);
        Rectangle srow = { x, y, w, srow_h };
        if (JW_CLICK == mode && ui_button_hit(srow, mx, my)) {
            s_section[sec].expanded = !s_section[sec].expanded;
            return y;
        }
        y += srow_h;

        if (!s_section[sec].expanded) continue;

        int pages = page_count(count);
        if (s_page[sec] >= pages) s_page[sec] = pages - 1;
        if (s_page[sec] < 0) s_page[sec] = 0;

        int start = s_page[sec] * QUEST_JOURNAL_PAGE_SIZE;
        int end   = start + QUEST_JOURNAL_PAGE_SIZE;
        if (end > count) end = count;

        for (int i = start; i < end; ++i) {
            const QuestProgressEntry* e = quest_progress_store_get((QuestStatus)sec, i);
            if (!e) continue;

            int cx = (int)(x + QJ_CARD_MX), cw = (int)(w - 2 * QJ_CARD_MX);
            int ch = quest_card_layout(JW_MEASURE, 0, 0, e, (QuestStatus)sec, cx, (int)y, cw);
            Rectangle card = { x + QJ_CARD_MX, y, w - 2 * QJ_CARD_MX, (float)ch };
            if (JW_DRAW == mode) {
                DrawRectangleRounded(card, 0.16f, 4, C_CARD);
                quest_card_layout(JW_DRAW, 0, 0, e, (QuestStatus)sec, cx, (int)y, cw);
            } else if (JW_CLICK == mode && ui_button_hit(card, mx, my)) {
                quest_card_layout(JW_CLICK, mx, my, e, (QuestStatus)sec, cx, (int)y, cw);
                return y;
            }
            y += ch + QJ_CARD_GAP;
        }

        /* Pagination — "<  Page N/M  >" with the shared ui-icon arrows; a
         * disabled arrow fades instead of disappearing so layout stays stable. */
        if (count > QUEST_JOURNAL_PAGE_SIZE) {
            Rectangle prev = { x + 8, y + 2, (float)QJ_PAGER_H, QJ_PAGER_H - 4 };
            Rectangle next = { x + w - QJ_PAGER_H - 8, y + 2, (float)QJ_PAGER_H, QJ_PAGER_H - 4 };
            bool can_prev = s_page[sec] > 0;
            bool can_next = s_page[sec] < pages - 1;
            if (JW_DRAW == mode) {
                Color on  = { 255, 255, 255, 235 };
                Color off = { 255, 255, 255, 70 };
                ui_icon_draw_ex("arrow-left", prev.x + prev.width * 0.5f,
                                prev.y + prev.height * 0.5f, 14.0f, 0.0f,
                                can_prev ? on : off);
                ui_icon_draw_ex("arrow-right", next.x + next.width * 0.5f,
                                next.y + next.height * 0.5f, 14.0f, 0.0f,
                                can_next ? on : off);
                char ctr[24];
                snprintf(ctr, sizeof(ctr), "Page %d/%d", s_page[sec] + 1, pages);
                int cw = MeasureText(ctr, QJ_FONT_SMALL);
                DrawText(ctr, (int)(x + (w - cw) / 2), (int)(y + 4), QJ_FONT_SMALL, C_DIM);
            } else if (JW_CLICK == mode && can_prev && ui_button_hit(prev, mx, my)) {
                s_page[sec]--;
                return y;
            } else if (JW_CLICK == mode && can_next && ui_button_hit(next, mx, my)) {
                s_page[sec]++;
                return y;
            }
            y += QJ_PAGER_H;
        }
    }
    return y;
}

/* ── Public API ───────────────────────────────────────────────────────── */

void quest_journal_init(void) {
    s_init    = false;
    set_visible(false);
    s_age     = 0.0f;
    for (int i = 0; i < QUEST_STATUS_COUNT; ++i) {
        s_page[i] = 0;
    }
}

/* Sections, pages and the scroll offset persist across toggles. */
void quest_journal_toggle(void) {
    set_visible(!s_visible);
    if (s_visible) {
        ensure_init();
        s_age = 0.0f;                          /* replay the panel pop on each open */
    }
}

bool quest_journal_is_visible(void) {
    return s_visible;
}

void quest_journal_update(float dt) {
    ensure_init();
    s_age += dt;
    if (s_visible) quest_target_update();
    for (int i = 0; i < QUEST_STATUS_COUNT; ++i) {
        ui_toggle_update(&s_section[i], dt);
    }

    /* Advance the section scroll (drag/glide/clamp) against last frame's
     * measure, then activate a section tap only on a clean, drag-less release. */
    ui_scroll_update(&s_scroll, s_view, s_content_h, dt);
    int cx, cy;
    if (ui_scroll_take_click(&s_scroll, &cx, &cy)) {
        Rectangle panel = panel_rect();
        sections_walk(JW_CLICK, cx, cy, panel.x,
                      panel.y + s_header_h - ui_scroll_offset(&s_scroll), panel.width);
    }
}

void quest_journal_draw(void) {
    if (!s_visible) return;
    ensure_init();
    /* Measure: fixed header, then the full (unclipped) section height. The
     * slot the column hands back is sized from that measure. */
    Rectangle probe = panel_rect();
    s_header_h = header_walk(JW_MEASURE, 0, 0, probe.x, probe.y, probe.width);
    s_content_h = sections_walk(JW_MEASURE, 0, 0, probe.x, probe.y + s_header_h, probe.width) - (probe.y + s_header_h);
    Rectangle panel = panel_rect();
    if (0.0f >= panel.height) {
        s_panel_h = 0.0f;
        s_view = (Rectangle){ 0 };
        return;
    }
    float x = panel.x, w = panel.width, y = panel.y;

    /* The header always draws so its close control stays reachable; the
     * sections scroll inside whatever room is left under it. */
    float view_h = panel.height - s_header_h;
    if (view_h < 0.0f) view_h = 0.0f;
    s_view = (Rectangle){ x, y + s_header_h, w, view_h };
    s_panel_h = s_header_h + view_h;

    /* The journal and minimap share the same settled panel transparency. */
    Rectangle full = { x, y, w, s_panel_h };
    float a = modal_pop_alpha(s_age);
    Color bg = { 5, 10, 22, 112 };
    bg.a = (unsigned char)(bg.a * a);
    DrawRectangleRec(full, bg);

    header_walk(JW_DRAW, 0, 0, x, y, w);

    if (view_h > 0.0f) {
        ui_scroll_begin(&s_scroll);
        sections_walk(JW_DRAW, 0, 0, x, y + s_header_h - ui_scroll_offset(&s_scroll), w);
        ui_scroll_end(&s_scroll);
    }
}

bool quest_journal_covers_point(int mx, int my) {
    if (!s_visible) return false;
    Rectangle panel = panel_rect();
    return ui_button_hit((Rectangle){ panel.x, panel.y, panel.width, s_panel_h }, mx, my);
}

bool quest_journal_handle_click(int mx, int my) {
    if (!quest_journal_covers_point(mx, my)) return false;

    /* The header is fixed and handled immediately; taps in the section area
     * arm the scroll gesture, with activation deferred to a clean release. */
    Rectangle panel = panel_rect();
    Rectangle header = { panel.x, panel.y, panel.width, s_header_h };
    if (ui_button_hit(header, mx, my)) {
        header_walk(JW_CLICK, mx, my, panel.x, panel.y, panel.width);
        return true;
    }
    ui_scroll_on_press(&s_scroll, mx, my);
    return true;
}

bool quest_journal_handle_wheel(float wheel_delta) {
    if (!s_visible) return false;
    return ui_scroll_on_wheel(&s_scroll, s_view, s_content_h, wheel_delta);
}
