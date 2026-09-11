#ifndef UI_STAT_PANEL_H
#define UI_STAT_PANEL_H
#include "domain/stat_contract_generated.h"

/* Stat display shared by the inventory modal and the interact modal. Every
 * block reads one canonical-order value array and returns the height it drew. */

#define STAT_PANEL_SUM_H 56.0f

/* Header row: the stats icon, the signed sum of `values`, and `label`. */
float stat_panel_sum_draw(float x, float y, float width, float pad,
                          const float values[CYBERIA_STAT_COUNT], const char* label);

/* Two-column grid: one icon, name and signed value per stat, at `font`. */
float stat_panel_grid_draw(float x, float y, float width, float pad, int font,
                           const float values[CYBERIA_STAT_COUNT]);

/* Header row followed by the grid. */
float stat_panel_draw(float x, float y, float width, float pad, int font,
                      const float values[CYBERIA_STAT_COUNT], const char* label);

#endif
