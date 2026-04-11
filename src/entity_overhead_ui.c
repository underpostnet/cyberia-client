/**
 * @file entity_overhead_ui.c
 * @brief World-space overhead UI — nameplate, capacity bar, HP bar.
 *
 * Rendering stack (drawn top → bottom, above the entity):
 *
 *   [nameplate text]                    ← topmost, white label with shadow
 *   [■■■■■■■■■■□□□□]  load bar          ← green→yellow gradient by fill %
 *   [■■■■■■■□□□□□□□]  HP bar            ← green fill, grey background
 *    "HP 73 / 100"                      ← label centred under HP bar
 *            │
 *         entity top edge
 *
 * All coordinates are in world space — this function is called inside
 * BeginMode2D so Raylib's camera transform is already applied.
 *
 * The module is fully stateless.  Call entity_overhead_ui_draw() with fresh
 * params every frame; no init/cleanup is required.
 */

#include "entity_overhead_ui.h"
#include "game_state.h"
#include "ui_icon.h"
#include <raylib.h>
#include <stdio.h>
#include <string.h>

/* ── Internal colours ────────────────────────────────────────────────── */

/* Bar backgrounds */
static const Color C_BAR_BG      = { 30,  30,  30, 100 };

/* HP bar fill */
static const Color C_HP_FULL     = { 70, 210,  70, 170 };   /* bright green */
static const Color C_HP_LOW      = {230,  60,  40, 170 };   /* red when ≤25% */
static const Color C_HP_MID      = {230, 175,  30, 170 };   /* amber at 50 % */

/* Load bar fill — green (low) → yellow (near limit) */
static const Color C_LEVEL_SAFE   = { 80, 200,  80, 170 };
static const Color C_LEVEL_WARN   = {230, 200,  30, 170 };
static const Color C_LEVEL_FULL   = {230, 100,  30, 170 };   /* orange at cap  */

/* Text colours */
static const Color C_NAME_TEXT   = {255, 255, 255, 230 };
static const Color C_NAME_SHADOW = {  0,   0,   0, 130 };
static const Color C_HP_LABEL    = {235, 235, 235, 230 };

/* ── Colour lerp helper ───────────────────────────────────────────────── */

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

/* ── Sub-component: nameplate ────────────────────────────────────────── */

/** Draws the display-name label, centred horizontally over the entity.
 *  @param cx  Screen-space centre X (pixels).
 *  @param sy  Screen-space Y of the bottom edge of the nameplate row.   */
static void draw_nameplate(const char *name, float cx, float sy) {
    if (!name || name[0] == '\0') return;

    int fs = EOHUD_NAME_FONT_SIZE;
    int tw = MeasureText(name, fs);
    int tx = (int)(cx - tw * 0.5f);
    int ty = (int)(sy - fs);

    /* Drop shadow */
    DrawText(name, tx + 1, ty + 1, fs, C_NAME_SHADOW);
    /* Main label */
    DrawText(name, tx, ty, fs, C_NAME_TEXT);
}

/* ── Sub-component: effective level bar ─────────────────────────────── */

/** Draws the effective-level bar.
 *  @param bar_rect  Screen-space rectangle for the full bar background. */
static void draw_level_bar(Rectangle bar_rect, int current, int max) {
    /* Background */
    DrawRectangleRec(bar_rect, C_BAR_BG);

    if (max <= 0 || current < 0) return;

    float frac = (float)current / (float)max;
    if (frac > 1.0f) frac = 1.0f;

    /* Colour: green → yellow → orange as level grows */
    Color fill;
    if (frac < 0.5f) {
        fill = color_lerp(C_LEVEL_SAFE, C_LEVEL_WARN, frac * 2.0f);
    } else {
        fill = color_lerp(C_LEVEL_WARN, C_LEVEL_FULL, (frac - 0.5f) * 2.0f);
    }

    Rectangle fill_rect = bar_rect;
    fill_rect.width = bar_rect.width * frac;
    if (fill_rect.width < 1.0f && current > 0) fill_rect.width = 1.0f;
    DrawRectangleRec(fill_rect, fill);

    /* 1-px border */
    DrawRectangleLinesEx(bar_rect, 1.0f, (Color){ 80, 80, 80, 100 });

    /* Level label: "Lv. X / Y" centred inside the bar */
    char label[32];
    snprintf(label, sizeof(label), "Lv. %d / %d", current, max);
    int lfs = EOHUD_LEVEL_LABEL_FONT_SIZE;
    int ltw = MeasureText(label, lfs);
    int lx  = (int)(bar_rect.x + bar_rect.width * 0.5f - ltw * 0.5f);
    int ly  = (int)(bar_rect.y + (bar_rect.height - lfs) * 0.5f);
    DrawText(label, lx + 1, ly + 1, lfs, (Color){ 0, 0, 0, 160 }); /* shadow */
    DrawText(label, lx,     ly,     lfs, C_HP_LABEL);
}

/* ── Sub-component: health bar ───────────────────────────────────────── */

/** Draws the HP bar + "HP cur / max" label below it.
 *  @param bar_rect  Screen-space rectangle for the full bar background.  */
static void draw_hp_bar(Rectangle bar_rect, float life, float max_life) {
    /* Background */
    DrawRectangleRec(bar_rect, C_BAR_BG);

    if (max_life <= 0.0f || life < 0.0f) return;

    float frac = life / max_life;
    if (frac > 1.0f) frac = 1.0f;

    /* Colour: green → amber → red as HP falls */
    Color fill;
    if (frac > 0.5f) {
        fill = color_lerp(C_HP_MID, C_HP_FULL, (frac - 0.5f) * 2.0f);
    } else {
        fill = color_lerp(C_HP_LOW, C_HP_MID, frac * 2.0f);
    }

    Rectangle fill_rect = bar_rect;
    fill_rect.width = bar_rect.width * frac;
    if (fill_rect.width < 1.0f && life > 0.0f) fill_rect.width = 1.0f;
    DrawRectangleRec(fill_rect, fill);

    /* 1-px border */
    DrawRectangleLinesEx(bar_rect, 1.0f, (Color){ 80, 80, 80, 100 });

    /* HP label: "HP 73 / 100" centred inside the bar */
    int ilif  = (int)(life + 0.5f);
    int imaxl = (int)(max_life + 0.5f);
    char label[32];
    snprintf(label, sizeof(label), "HP %d / %d", ilif, imaxl);
    int lfs = EOHUD_HP_LABEL_FONT_SIZE;
    int ltw = MeasureText(label, lfs);
    int lx  = (int)(bar_rect.x + bar_rect.width * 0.5f - ltw * 0.5f);
    int ly  = (int)(bar_rect.y + (bar_rect.height - lfs) * 0.5f);
    DrawText(label, lx + 1, ly + 1, lfs, (Color){ 0, 0, 0, 160 }); /* shadow */
    DrawText(label, lx,     ly,     lfs, C_HP_LABEL);
}

/* ── Public API ─────────────────────────────────────────────────────── */

void entity_overhead_ui_draw(
    const EntityOverheadParams *p,
    float world_x,
    float world_y,
    float world_w,
    float world_h,
    float cell_size)
{
    (void)world_h; /* height not used for anchor; bars sit above top edge */

    /* Convert entity top edge and horizontal centre to screen pixels. */
    float entity_top_px = world_y * cell_size;
    float entity_cx_px  = (world_x + world_w * 0.5f) * cell_size;

    /* Bar pixel width (wider than the entity by BAR_WIDTH_RATIO). */
    float bar_w_px = world_w * cell_size * EOHUD_BAR_WIDTH_RATIO;
    float bar_h_px = EOHUD_BAR_HEIGHT * cell_size;

    /* Stack cursor: start above the entity top edge and grow upward. */
    float cursor_px = entity_top_px - EOHUD_GAP_ABOVE_ENTITY * cell_size;

    /* ── HP bar (closest to entity) ──────────────────────────────────── */
    if (p->show_hp && p->max_life > 0.0f) {
        /* Place bar so its bottom is at cursor_px. */
        cursor_px -= bar_h_px;
        Rectangle hprect = {
            entity_cx_px - bar_w_px * 0.5f,
            cursor_px,
            bar_w_px,
            bar_h_px
        };
        draw_hp_bar(hprect, p->life, p->max_life);
        cursor_px -= EOHUD_BAR_SPACING * cell_size;
    }

    /* ── Level bar (above HP bar) ────────────────────────────────────── */
    if (p->show_level && p->max_level > 0) {
        cursor_px -= bar_h_px;
        Rectangle ldrect = {
            entity_cx_px - bar_w_px * 0.5f,
            cursor_px,
            bar_w_px,
            bar_h_px
        };
        draw_level_bar(ldrect, p->effective_level, p->max_level);
    }

    /* ── Nameplate (topmost) ──────────────────────────────────────────── */
    if (p->show_name) {
        cursor_px -= EOHUD_NAME_SPACING * cell_size;
        draw_nameplate(p->name, entity_cx_px, cursor_px);
        cursor_px -= (float)EOHUD_NAME_FONT_SIZE;
    }

    /* ── Status icon (above nameplate) ────────────────────────────────── */
    if (p->status_icon != 0) {
        const char* icon_id = NULL;
        for (int i = 0; i < g_game_state.status_icon_count; i++) {
            if (g_game_state.status_icons[i].id == p->status_icon) {
                icon_id = g_game_state.status_icons[i].icon_id;
                break;
            }
        }
        if (icon_id) {
            cursor_px -= EOHUD_ICON_SPACING * cell_size;
            /* Scale icon with zoom: roughly 55% of one cell, clamped. */
            int icon_sz = (int)(0.55f * world_w * cell_size);
            if (icon_sz < 20) icon_sz = 20;
            if (icon_sz > 36) icon_sz = 36;
            /* Phase offset: simple hash of entity name for desync. */
            float phase = 0.0f;
            if (p->name) {
                unsigned int h = 0;
                for (const char* c = p->name; *c; c++) h = h * 31 + (unsigned char)*c;
                phase = (float)(h % 1000) * 0.001f * 6.2832f;
            }
            ui_icon_draw(icon_id, entity_cx_px, cursor_px - icon_sz * 0.5f,
                         icon_sz, true, phase);
        }
    }
}
