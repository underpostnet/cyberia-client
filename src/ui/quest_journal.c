#include "quest_journal.h"

#include "modal.h"
#include "quest_store.h"
#include "ui_button.h"
#include "ui_toggle.h"

#include <raylib.h>
#include <stdio.h>
#include <string.h>

/* ── Layout ───────────────────────────────────────────────────────────── */

#define QJ_PANEL_W        220
#define QJ_MARGIN         8
#define QJ_TOP_OFFSET     130   /* below the upper-right map info modal */
#define QJ_HEADER_H       40
#define QJ_SECTION_H      36
#define QJ_ROW_H          22
#define QJ_DETAIL_H       54
#define QJ_PAGER_H        22
#define QJ_CHEVRON        32
#define QJ_SMALL_SCREEN_BREAKPOINT_PX 600

#define QJ_FONT_TITLE     16
#define QJ_FONT_SECTION   14
#define QJ_FONT_ROW       13
#define QJ_FONT_SMALL     11

/* Header / detail sit on top of the shared MODAL_PANEL_BG fill. */
static const Color C_HEADER   = {  24,  30,  48, 255 };
static const Color C_TEXT     = { 220, 220, 230, 240 };
static const Color C_DIM      = { 140, 140, 160, 200 };
static const Color C_DETAIL   = {  22,  22,  34, 255 };
static const Color C_DIS      = {  90,  90, 110, 160 };

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

static bool     s_init = false;
static float    s_age  = 0.0f;
static float    s_panel_h = QJ_HEADER_H;
static UIToggle s_panel;
static UIToggle s_section[QUEST_STATUS_COUNT];
static int      s_page[QUEST_STATUS_COUNT]   = { 0, 0, 0 };
static int      s_detail[QUEST_STATUS_COUNT] = { -1, -1, -1 };

static int page_count(int count) {
    if (count <= 0) return 1;
    return (count + QUEST_JOURNAL_PAGE_SIZE - 1) / QUEST_JOURNAL_PAGE_SIZE;
}

static Rectangle panel_rect(void) {
    int sw = GetScreenWidth();
    return (Rectangle){ (float)(sw - QJ_PANEL_W - QJ_MARGIN), (float)QJ_TOP_OFFSET,
                        (float)QJ_PANEL_W, 0.0f };
}

static bool hit(int mx, int my, Rectangle r) {
    return ((float)mx >= r.x && (float)mx < r.x + r.width &&
            (float)my >= r.y && (float)my < r.y + r.height);
}

static void ensure_init(void) {
    if (s_init) return;
    bool expanded = GetScreenWidth() >= QJ_SMALL_SCREEN_BREAKPOINT_PX;
    Rectangle z = { 0, 0, QJ_CHEVRON, QJ_CHEVRON };
    ui_toggle_init(&s_panel, z, expanded, UI_TOGGLE_CHEVRON_UP);
    for (int i = 0; i < QUEST_STATUS_COUNT; ++i) {
        ui_toggle_init(&s_section[i], z, i == QUEST_ACTIVE, UI_TOGGLE_CHEVRON_DOWN);
    }
    s_init = true;
}

/* ── Unified layout walk (single source of truth) ─────────────────────── */
/*
 * JW_DRAW renders, JW_CLICK hit-tests + applies actions, JW_MEASURE only
 * advances the y cursor to size the panel (so the shared background can be
 * filled before the content is drawn). Returns true in JW_CLICK when a tap
 * was consumed. Always records the final panel height in s_panel_h.
 */
static bool journal_walk(int mode, int mx, int my) {
    ensure_init();

    Rectangle panel = panel_rect();
    float x = panel.x;
    float w = panel.width;
    float y = panel.y;

    Rectangle header = { x, y, w, QJ_HEADER_H };
    Rectangle ptoggle = { x + w - QJ_CHEVRON - 6, y + (QJ_HEADER_H - QJ_CHEVRON) / 2,
                          QJ_CHEVRON, QJ_CHEVRON };
    ui_toggle_set_anchor(&s_panel, ptoggle);

    if (JW_DRAW == mode) {
        DrawRectangleRec(header, C_HEADER);
        DrawText("Quest Journal", (int)(x + 8), (int)(y + (QJ_HEADER_H - QJ_FONT_TITLE) / 2),
                 QJ_FONT_TITLE, C_TEXT);
        ui_toggle_draw(&s_panel);
    } else if (JW_CLICK == mode && hit(mx, my, header)) {
        s_panel.expanded = !s_panel.expanded; /* tap anywhere on header */
        return true;
    }

    y += QJ_HEADER_H;

    if (!s_panel.expanded) {
        s_panel_h = y - panel.y;
        return false;
    }

    for (int sec = 0; sec < QUEST_STATUS_COUNT; ++sec) {
        int count = quest_store_count((QuestStatus)sec);

        Rectangle srow = { x, y, w, QJ_SECTION_H };
        Rectangle schev = { x + 6, y + (QJ_SECTION_H - QJ_CHEVRON) / 2, QJ_CHEVRON, QJ_CHEVRON };
        ui_toggle_set_anchor(&s_section[sec], schev);

        if (JW_DRAW == mode) {
            ui_toggle_draw(&s_section[sec]);
            char label[64];
            snprintf(label, sizeof(label), "%s (%d)", C_SECTION_LABEL[sec], count);
            DrawText(label, (int)(x + 6 + QJ_CHEVRON + 6),
                     (int)(y + (QJ_SECTION_H - QJ_FONT_SECTION) / 2),
                     QJ_FONT_SECTION, count > 0 ? C_TEXT : C_DIM);
        } else if (JW_CLICK == mode && hit(mx, my, srow)) {
            s_section[sec].expanded = !s_section[sec].expanded;
            return true;
        }
        y += QJ_SECTION_H;

        if (!s_section[sec].expanded) continue;

        int pages = page_count(count);
        if (s_page[sec] >= pages) s_page[sec] = pages - 1;
        if (s_page[sec] < 0) s_page[sec] = 0;

        int start = s_page[sec] * QUEST_JOURNAL_PAGE_SIZE;
        int end   = start + QUEST_JOURNAL_PAGE_SIZE;
        if (end > count) end = count;

        for (int i = start; i < end; ++i) {
            const QuestEntry* e = quest_store_get((QuestStatus)sec, i);
            if (!e) continue;

            Rectangle row = { x, y, w, QJ_ROW_H };
            if (JW_DRAW == mode) {
                DrawCircle((int)(x + 12), (int)(y + QJ_ROW_H / 2), 4.0f, C_STATUS[sec]);
                DrawText(e->title, (int)(x + 24),
                         (int)(y + (QJ_ROW_H - QJ_FONT_ROW) / 2), QJ_FONT_ROW, C_TEXT);
            } else if (JW_CLICK == mode && hit(mx, my, row)) {
                s_detail[sec] = (s_detail[sec] == i) ? -1 : i; /* one open per section */
                return true;
            }
            y += QJ_ROW_H;

            if (s_detail[sec] == i) {
                Rectangle det = { x, y, w, QJ_DETAIL_H };
                if (JW_DRAW == mode) {
                    DrawRectangleRec(det, C_DETAIL);
                    DrawText(e->description, (int)(x + 10), (int)(y + 4), QJ_FONT_SMALL, C_DIM);
                    if (e->active_step[0] != '\0') {
                        DrawText(e->active_step, (int)(x + 10), (int)(y + 20), QJ_FONT_SMALL, C_TEXT);
                    }
                    if (e->objectives[0] != '\0') {
                        DrawText(e->objectives, (int)(x + 10), (int)(y + 36), QJ_FONT_SMALL, C_DIM);
                    }
                }
                y += QJ_DETAIL_H;
            }
        }

        /* Pagination — only when expanded and count exceeds one page. */
        if (count > QUEST_JOURNAL_PAGE_SIZE) {
            Rectangle prev = { x + 8, y + 2, 54, QJ_PAGER_H - 4 };
            Rectangle next = { x + w - 54 - 8, y + 2, 54, QJ_PAGER_H - 4 };
            bool can_prev = s_page[sec] > 0;
            bool can_next = s_page[sec] < pages - 1;
            if (JW_DRAW == mode) {
                UIButtonStyle pager = { .font_size = QJ_FONT_SMALL, .no_fill = true,
                                        .text_color = C_TEXT, .text_disabled = C_DIS };
                pager.text = "Prev";
                ui_button_draw(prev, &pager, ui_button_resolve_state(can_prev, false, false));
                pager.text = "Next";
                ui_button_draw(next, &pager, ui_button_resolve_state(can_next, false, false));
                char ctr[24];
                snprintf(ctr, sizeof(ctr), "Page %d / %d", s_page[sec] + 1, pages);
                int cw = MeasureText(ctr, QJ_FONT_SMALL);
                DrawText(ctr, (int)(x + (w - cw) / 2), (int)(y + 4), QJ_FONT_SMALL, C_DIM);
            } else if (JW_CLICK == mode && can_prev && hit(mx, my, prev)) {
                s_page[sec]--;
                s_detail[sec] = -1;
                return true;
            } else if (JW_CLICK == mode && can_next && hit(mx, my, next)) {
                s_page[sec]++;
                s_detail[sec] = -1;
                return true;
            }
            y += QJ_PAGER_H;
        }
    }

    s_panel_h = y - panel.y;
    return false;
}

/* ── Public API ───────────────────────────────────────────────────────── */

void quest_journal_init(void) {
    s_init = false;
    s_age  = 0.0f;
    for (int i = 0; i < QUEST_STATUS_COUNT; ++i) {
        s_page[i]   = 0;
        s_detail[i] = -1;
    }
}

void quest_journal_update(float dt) {
    ensure_init();
    s_age += dt;
    ui_toggle_update(&s_panel, dt);
    for (int i = 0; i < QUEST_STATUS_COUNT; ++i) {
        ui_toggle_update(&s_section[i], dt);
    }
}

void quest_journal_draw(void) {
    ensure_init();
    Rectangle panel = panel_rect();
    journal_walk(JW_MEASURE, 0, 0);                       /* size the panel */
    Rectangle full = { panel.x, panel.y, panel.width, s_panel_h };
    modal_draw_panel(full, s_age);                        /* shared dark fill + fade-in */
    journal_walk(JW_DRAW, 0, 0);                          /* content on top */
}

bool quest_journal_handle_click(int mx, int my) {
    Rectangle panel = panel_rect();
    Rectangle bounds = { panel.x, panel.y, panel.width, s_panel_h };
    if (!hit(mx, my, bounds)) return false;
    return journal_walk(JW_CLICK, mx, my);
}
