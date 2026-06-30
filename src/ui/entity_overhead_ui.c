/**
 * @file entity_overhead_ui.c
 * @brief World-space overhead UI — HP bar, nameplate, capability bar, presence.
 *
 * Rendering stack (drawn bottom → top, above the entity):
 *
 *   [presence icon]                      ← standalone lifecycle icon (unchanged)
 *   ( Σ )[action][quest]   capability bar ← Σ-stats circle + capability icons
 *   [ Display Name ]       nameplate
 *   [ HP 73 / 100  ]       HP bar
 *            │
 *         entity top edge
 *
 * The three pill rows (HP bar, nameplate, capability bar) share one background,
 * height, padding, and rounding, and are sized in fixed screen pixels so they
 * are uniform for every entity regardless of its world size. Only the vertical
 * anchor above the entity tracks world space. Stateless — call per entity per
 * frame inside BeginMode2D.
 */

#include "entity_overhead_ui.h"
#include "text.h"

#include "domain/presentation_runtime.h"
#include "ui_icon.h"
#include "world_types.h"

#include <raylib.h>
#include <stdio.h>

/* ── Colours ─────────────────────────────────────────────────────────── */

/* HP bar fill — green → amber → red as HP falls. */
static const Color C_HP_FULL     = { 50, 220,  50, 240 };
static const Color C_HP_LOW      = {230,  50,  40, 240 };
static const Color C_HP_MID      = {200, 180,  30, 240 };

/* Nameplate pill backdrop + the subtle border. */
static const Color C_PILL_BG     = {  4,   4,  12, 150 };
static const Color C_PILL_BORDER = {120, 120, 120, 160 };

/* HP bar — black background (the depleted portion reads as black) + outline. */
static const Color C_HP_BG       = {  0,   0,   0, 235 };
static const Color C_HP_BORDER   = {  0,   0,   0, 235 };

/* Text. */
static const Color C_NAME_TEXT   = {255, 255, 255, 255 };
static const Color C_NAME_SHADOW = {  0,   0,   0, 200 };
static const Color C_LABEL       = {255, 255, 255, 245 };
static const Color C_LABEL_SHADOW = {  0,   0,   0, 200 };

/* Stronger, fully-opaque shadow for the sum-of-stats value only. */
static const Color C_STAT_SHADOW = {  0,   0,   0, 255 };

static Color color_lerp(Color a, Color b, float t) {
    if (t <= 0.0f) return a;
    if (t >= 1.0f) return b;
    return (Color){
        (unsigned char)(a.r + (float)(b.r - a.r) * t),
        (unsigned char)(a.g + (float)(b.g - a.g) * t),
        (unsigned char)(a.b + (float)(b.b - a.b) * t),
        (unsigned char)(a.a + (float)(b.a - a.a) * t),
    };
}

/* ── Shared pill ─────────────────────────────────────────────────────── */

/** Draws the standardized pill (dark rounded backdrop + subtle border) sized to
 *  `content_w` at the fixed row height, centred at cx with its top at top_y.
 *  Returns the inner content rectangle. */
static Rectangle draw_pill(float cx, float top_y, float content_w) {
    float pill_w = content_w + EOHUD_PILL_PAD_X * 2;
    Rectangle pill = { cx - pill_w * 0.5f, top_y, pill_w, (float)EOHUD_BAR_H };
    DrawRectangleRounded(pill, EOHUD_PILL_ROUND, 8, C_PILL_BG);
    DrawRectangleRoundedLinesEx(pill, EOHUD_PILL_ROUND, 8, 1.0f, C_PILL_BORDER);
    return (Rectangle){ cx - content_w * 0.5f, top_y, content_w, (float)EOHUD_BAR_H };
}

/* Centre `label` (with shadow) vertically inside a row whose top is top_y. */
static void draw_centered_label(const char *label, float cx, float top_y, int fs,
                                Color fg, Color shadow) {
    int tw = MeasureText(label, fs);
    int tx = (int)(cx - tw * 0.5f);
    int ty = (int)(top_y + (EOHUD_BAR_H - fs) * 0.5f);
    DrawText(label, tx + 1, ty + 1, fs, shadow);
    DrawText(label, tx, ty, fs, fg);
}

/* ── Rows ────────────────────────────────────────────────────────────── */

static void draw_hp_bar(float cx, float top_y, float life, float max_life) {
    Rectangle bar = { cx - EOHUD_HP_BAR_W * 0.5f, top_y,
                      (float)EOHUD_HP_BAR_W, (float)EOHUD_BAR_H };

    /* Black background so the depleted (subtracted) HP reads as black. */
    DrawRectangleRounded(bar, EOHUD_PILL_ROUND, 8, C_HP_BG);

    if (max_life > 0.0f && life >= 0.0f) {
        float frac = life / max_life;
        if (frac > 1.0f) frac = 1.0f;
        Color fill = (frac > 0.5f)
            ? color_lerp(C_HP_MID, C_HP_FULL, (frac - 0.5f) * 2.0f)
            : color_lerp(C_HP_LOW, C_HP_MID, frac * 2.0f);
        Rectangle fr = bar;
        fr.width = bar.width * frac;
        if (fr.width < 1.0f && life > 0.0f) fr.width = 1.0f;
        DrawRectangleRounded(fr, EOHUD_PILL_ROUND, 8, fill);
    }
    DrawRectangleRoundedLinesEx(bar, EOHUD_PILL_ROUND, 8, 1.0f, C_HP_BORDER);

    int ilif = (int)(life + 0.5f), imaxl = (int)(max_life + 0.5f);
    char label[32];
    snprintf(label, sizeof(label), "HP %d / %d", ilif, imaxl);
    draw_centered_label(label, cx, top_y, EOHUD_HP_LABEL_FONT_SIZE, C_LABEL, C_LABEL_SHADOW);
}

static void draw_nameplate(const char *name, float cx, float top_y) {
    if (!name || name[0] == '\0') return;
    int tw = MeasureText(name, EOHUD_NAME_FONT_SIZE);
    draw_pill(cx, top_y, (float)tw);
    draw_centered_label(name, cx, top_y, EOHUD_NAME_FONT_SIZE, C_NAME_TEXT, C_NAME_SHADOW);
}

/** Capability bar: an optional leading 'stats' icon + outlined sum-of-stats value
 *  (no fill — the strong glyph outline carries it), then one icon per set
 *  interaction-capability bit (action, quest). `show_value` gates only the
 *  Σ-stats lead; the capability icons always render for the set flags. */
static void draw_capability_bar(float cx, float top_y, int stats_sum,
                                bool show_value, uint8_t flags, float phase) {
    const char *icons[2];
    int icon_n = 0;
    if (flags & INTERACTION_FLAG_ACTION)
        icons[icon_n++] = presentation_runtime_status_icon(STATUS_ICON_ACTION_PROVIDER);
    if (flags & INTERACTION_FLAG_QUEST)
        icons[icon_n++] = presentation_runtime_status_icon(STATUS_ICON_QUEST_PROVIDER);

    int fs = EOHUD_STATS_FONT_SIZE;
    char num[16];
    int tw = 0;
    if (show_value) {
        snprintf(num, sizeof(num), "%d", stats_sum);
        tw = MeasureText(num, fs);
    }

    /* Total width: optional Σ-stats lead (icon + value) plus one icon per
     * capability flag, each element separated by EOHUD_ITEM_GAP. */
    float content_w = 0.0f;
    bool acc = false;
    if (show_value) {
        content_w += (float)EOHUD_CAP_ICON_SIZE + (float)EOHUD_ITEM_GAP + (float)tw;
        acc = true;
    }
    for (int i = 0; i < icon_n; i++) {
        if (acc) content_w += (float)EOHUD_ITEM_GAP;
        content_w += (float)EOHUD_CAP_ICON_SIZE;
        acc = true;
    }
    if (content_w <= 0.0f) return;

    float row_cy = top_y + EOHUD_BAR_H * 0.5f;
    float x = cx - content_w * 0.5f;
    bool drew = false;

    if (show_value) {
        ui_icon_draw("stats", x + EOHUD_CAP_ICON_SIZE * 0.5f, row_cy,
                     EOHUD_CAP_ICON_SIZE, false, phase);
        x += EOHUD_CAP_ICON_SIZE + EOHUD_ITEM_GAP;

        int nx = (int)x;
        int ny = (int)(row_cy - fs * 0.5f);
        /* Strong, gapless outline: two concentric 8-direction rings (1px then 2px)
         * so the shadow abuts the glyph with no translucent gap, then the value. */
        for (int o = 1; o <= 2; o++)
            for (int dy = -1; dy <= 1; dy++)
                for (int dx = -1; dx <= 1; dx++)
                    if (dx || dy)
                        DrawText(num, nx + dx * o, ny + dy * o, fs, C_STAT_SHADOW);
        DrawText(num, nx, ny, fs, C_LABEL);
        x += (float)tw;
        drew = true;
    }

    for (int i = 0; i < icon_n; i++) {
        if (drew) x += EOHUD_ITEM_GAP;
        if (icons[i] && icons[i][0] != '\0')
            ui_icon_draw(icons[i], x + EOHUD_CAP_ICON_SIZE * 0.5f, row_cy,
                         EOHUD_CAP_ICON_SIZE, true, phase);
        x += EOHUD_CAP_ICON_SIZE;
        drew = true;
    }
}

/* ── Public API ──────────────────────────────────────────────────────── */

void entity_overhead_ui_draw(
    const EntityOverheadParams *p,
    float world_x,
    float world_y,
    float world_w,
    float world_h,
    float cell_size)
{
    float entity_top_px = world_y * cell_size;
    float entity_cx_px  = (world_x + world_w * 0.5f) * cell_size;

    /* Anchor above the entity; rows stack upward in fixed pixels. */
    float cursor_px = entity_top_px - EOHUD_GAP_ABOVE_ENTITY * cell_size;

    float phase = 0.0f;
    if (p->name) {
        unsigned int h = 0;
        for (const char* c = p->name; *c; c++) h = h * 31 + (unsigned char)*c;
        phase = (float)(h % 1000) * 0.001f * 6.2832f;
    }

    if (p->show_hp && p->max_life > 0.0f) {
        cursor_px -= EOHUD_BAR_H;
        draw_hp_bar(entity_cx_px, cursor_px, p->life, p->max_life);
        cursor_px -= EOHUD_ROW_GAP;
    }

    if (p->show_name) {
        cursor_px -= EOHUD_BAR_H;
        draw_nameplate(p->name, entity_cx_px, cursor_px);
        cursor_px -= EOHUD_ROW_GAP;
    }

    if (p->show_stats) {
        cursor_px -= EOHUD_BAR_H;
        draw_capability_bar(entity_cx_px, cursor_px, p->stats_sum, p->show_stats_value, p->interaction_flags, phase);
        cursor_px -= EOHUD_ROW_GAP;
    }

    /* Presence status icon — topmost, unchanged behaviour (standalone bouncing
     * icon, no pill), now at a uniform size. */
    if (p->status_icon != 0) {
        const char* icon_id = presentation_runtime_status_icon(p->status_icon);
        if (icon_id && icon_id[0] != '\0') {
            cursor_px -= EOHUD_PRESENCE_SIZE;
            ui_icon_draw(icon_id, entity_cx_px, cursor_px + EOHUD_PRESENCE_SIZE * 0.5f,
                         EOHUD_PRESENCE_SIZE, true, phase);
        }
    }
}
