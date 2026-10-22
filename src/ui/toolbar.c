#include "toolbar.h"
#include "ease.h"

#include "chat_panel.h"
#include "hud_minimap_overlay.h"
#include "inventory_modal.h"
#include "modal_instance_map.h"
#include "modal_interact.h"
#include "modal_map.h"
#include "player_panel.h"
#include "quest_journal.h"
#include "save_status.h"
#include "session_gate.h"
#include "social_panel.h"
#include "text.h"
#include "ui_button.h"
#include "ui_icon.h"

#include "sim/local_player.h"

#include <math.h>
#include <raylib.h>
#include <stdio.h>

#define TOOLBAR_SLIDE_DURATION 0.22f

static const Color TB_BG   = { 8, 12, 24, 150 };
static const Color TB_LINE = { 70, 190, 240, 60 };

/* Slide parameter: 0 = strip fully visible, 1 = fully hidden. */
static bool  s_hidden  = false;
static float s_slide_t = 0.0f;

static float slide_eased(void) {
    float t = s_slide_t;
    return ease_smoothstep(t);
}

float toolbar_height(void)   { return TOOLBAR_H * (1.0f - slide_eased()); }
float toolbar_offset_y(void) { return -TOOLBAR_H * slide_eased(); }

/* Buttons of the toggle row: chat, social, quest, map, settings. */
#define TOOLBAR_TOGGLE_COUNT 5

/* Toggle row, right-aligned: index 0 is the rightmost button (settings).
 * All ride the strip's slide. */
static Rectangle btn_rect(int index_from_right) {
    float x = (float)GetScreenWidth()
              - (float)(index_from_right + 1) * (TOOLBAR_BTN_SIZE + TOOLBAR_BTN_MARGIN);
    float y = (TOOLBAR_H - TOOLBAR_BTN_SIZE) * 0.5f + toolbar_offset_y();
    return (Rectangle){ x, y, TOOLBAR_BTN_SIZE, TOOLBAR_BTN_SIZE };
}

float toolbar_row_left(void) { return btn_rect(TOOLBAR_TOGGLE_COUNT - 1).x; }

/* Hide/show toggle — top-left corner, pinned to the screen so the strip can
 * always be brought back.  Nearly full toolbar height with minimal edge
 * margin so the main logo icon has maximum presence. */
static Rectangle toggle_rect(void) {
    float sz = TOOLBAR_H - TOGGLE_EDGE_MARGIN * 2.0f;
    return (Rectangle){ TOGGLE_EDGE_MARGIN,
                        TOGGLE_EDGE_MARGIN,
                        sz, sz };
}

static void toolbar_button_draw(Rectangle r, const char* icon, bool selected) {
    /* Clean icon-only button: draw the icon directly with no background,
     * no border, no shadow. One white ring marks the hover or the selection. */
    float sz = r.width < r.height ? r.width : r.height;
    float cx = r.x + r.width * 0.5f;
    float cy = r.y + r.height * 0.5f;
    ui_icon_draw(icon, cx, cy, (int)(sz * 0.65f), false, 0.0f);
    if (selected || CheckCollisionPointRec(GetMousePosition(), r)) ui_button_draw_ring(r, WHITE);
}

static bool interact_inventory_chain_is_open(void) {
    return modal_interact_is_open() || inventory_modal_is_open();
}

static void close_interact_inventory_chain(void) {
    if (!interact_inventory_chain_is_open()) return;
    inventory_modal_set_on_close(NULL);
    modal_interact_close();
    inventory_modal_close();
}

/* The minimap and the quest journal share the right-hand column and stay
 * open together. Any world-covering modal gives way to whichever is tapped;
 * a tap that only dismissed one leaves the panel it targets open. */
/* Closes whatever covers the world and reports whether anything was open.
 * A tap that only dismissed a cover leaves the panel it targets open. */
static bool dismiss_world_covers(void) {
    bool dismissed = interact_inventory_chain_is_open() || modal_instance_map_is_open();
    close_interact_inventory_chain();
    modal_instance_map_close();
    return dismissed;
}

/* The social panel takes the whole column and covers the chat: a tap on the
 * chat closes it, and the chat shows. */
static bool dismiss_social_panel(void) {
    bool dismissed = social_panel_is_visible();
    social_panel_close();
    return dismissed;
}

/* The chat and the social panel cover the minimap and the journal: a tap on
 * either closes them, and the panel it targets shows. */
static bool dismiss_column_covers(void) {
    bool dismissed = dismiss_social_panel();
    if (!chat_panel_is_visible()) return dismissed;
    chat_panel_toggle();
    return true;
}

static void toggle_minimap(void) {
    bool dismissed = dismiss_world_covers();
    dismissed = dismiss_column_covers() || dismissed;
    if (dismissed || !hud_minimap_overlay_is_visible()) {
        if (!hud_minimap_overlay_is_visible()) hud_minimap_overlay_show();
        return;
    }
    hud_minimap_overlay_hide();
}

static void toggle_quest_journal(void) {
    bool dismissed = dismiss_world_covers();
    dismissed = dismiss_column_covers() || dismissed;
    if (dismissed || !quest_journal_is_visible()) {
        if (!quest_journal_is_visible()) quest_journal_toggle();
        return;
    }
    quest_journal_toggle();
}

static void toggle_chat_panel(void) {
    bool dismissed = dismiss_world_covers();
    dismissed = dismiss_social_panel() || dismissed;
    if (dismissed && chat_panel_is_visible()) return;
    chat_panel_toggle();
}

static void toggle_social_panel(void) {
    bool dismissed = dismiss_world_covers();
    if (dismissed && social_panel_is_visible()) return;
    social_panel_toggle();
}

static void toolbar_bar_draw(Rectangle r) {
    DrawRectangleRec(r, TB_BG);
    DrawLine((int)r.x, (int)(r.y + r.height), (int)(r.x + r.width), (int)(r.y + r.height), TB_LINE);
}

void toolbar_badge_draw(Rectangle r, int count) {
    if (0 >= count) return;
    char text[8];
    snprintf(text, sizeof text, "%d", 99 < count ? 99 : count);
    float cx = r.x + r.width - 3.0f;
    float cy = r.y + 4.0f;
    DrawCircle((int)cx, (int)cy, 8.0f, (Color){ 210, 60, 60, 245 });
    DrawText(text, (int)(cx - (float)MeasureText(text, 10) * 0.5f), (int)(cy - 5.0f), 10, WHITE);
}

void toolbar_draw(int screen_width) {
    /* Advance the slide (draw runs once per frame). */
    float dt = GetFrameTime();
    float step = dt / TOOLBAR_SLIDE_DURATION;
    s_slide_t += s_hidden ? step : -step;
    if (s_slide_t < 0.0f) s_slide_t = 0.0f;
    if (s_slide_t > 1.0f) s_slide_t = 1.0f;

    float oy = toolbar_offset_y();

    if (s_slide_t < 1.0f) {
        toolbar_bar_draw((Rectangle){ 0, oy, (float)screen_width, TOOLBAR_H });

        bool social = social_panel_is_visible();
        bool chat = chat_panel_is_visible() && !social;
        toolbar_button_draw(btn_rect(0), "engine", session_gate_settings_open());
        toolbar_button_draw(btn_rect(1), "map", hud_minimap_overlay_is_visible() && !social && !chat);
        toolbar_button_draw(btn_rect(2), "quest", quest_journal_is_visible() && !social && !chat);
        toolbar_button_draw(btn_rect(3), "friends", social);
        toolbar_badge_draw(btn_rect(3), local_player_social_news());
        toolbar_button_draw(btn_rect(4), "chat", chat);
        toolbar_badge_draw(btn_rect(4), chat_panel_unread());
    }

    toolbar_button_draw(toggle_rect(), s_hidden ? "cyberia-white-0" : "cyberia-yellow-1", false);
    save_status_draw(screen_width, toolbar_height() + 6.0f);
}

bool toolbar_handle_click(int mx, int my) {
    if (ui_button_hit(toggle_rect(), mx, my)) {
        s_hidden = !s_hidden;
        return true;
    }
    if (s_hidden) return false;
    if (ui_button_hit(btn_rect(0), mx, my)) { session_gate_settings(); return true; }
    if (ui_button_hit(btn_rect(1), mx, my)) {
        toggle_minimap();
        return true;
    }
    if (ui_button_hit(btn_rect(4), mx, my)) {
        toggle_chat_panel();
        return true;
    }
    if (ui_button_hit(btn_rect(3), mx, my)) {
        toggle_social_panel();
        return true;
    }
    if (ui_button_hit(btn_rect(2), mx, my)) {
        toggle_quest_journal();
        return true;
    }
    /* The coordinate readout opens the expanded inspection map. */
    if (ui_button_hit(modal_map_bounds(), mx, my)) {
        modal_instance_map_toggle();
        return true;
    }
    return false;
}

bool toolbar_covers_point(int mx, int my) {
    if (ui_button_hit(toggle_rect(), mx, my)) return true;
    return !s_hidden && 0 <= my && (float)my < toolbar_height() &&
           0 <= mx && mx < GetScreenWidth();
}
