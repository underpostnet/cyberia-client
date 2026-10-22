#ifndef CYBERIA_UI_TABS_H
#define CYBERIA_UI_TABS_H

#include <raylib.h>
#include <stdbool.h>

/* ui_tabs — the tab group of every panel: a grid of equal pixel-retro tabs.
 *
 * A wrapped group puts at most UI_TABS_ROW_MAX tabs in a row, and fewer when a
 * tab would not fit its icon and its label. Rows share the tabs evenly, so
 * nine tabs make three rows of three. */

#define UI_TABS_ROW_MAX 4
#define UI_TABS_GAP     4.0f

/* One tab. `badge` shows while the tab is not active. */
typedef struct {
    const char* label;
    const char* icon;
    bool        enabled;
    int         badge;
} UITab;

/* The geometry of a group: `columns` tabs in a row from (x, y). */
typedef struct {
    float x, y, tab_w, tab_h;
    int   count, columns;
} UITabs;

/* Label font of every tab group. */
int       ui_tabs_font(void);
/* Lays the tabs out in rows across `width` from (x, y). */
UITabs    ui_tabs_wrap(const UITab* tabs, int count, float x, float y, float width, float tab_h);
/* Lays `count` tabs out in one row across `width`. */
UITabs    ui_tabs_row(int count, float x, float y, float width, float tab_h);
float     ui_tabs_height(const UITabs* group);
Rectangle ui_tabs_rect(const UITabs* group, int index);
/* The index of the tab under the point, or -1. */
int       ui_tabs_hit(const UITabs* group, int mx, int my);
/* Icons show only while every tab fits its icon and its label. */
void      ui_tabs_draw(const UITabs* group, const UITab* tabs, int active);

#endif /* CYBERIA_UI_TABS_H */
