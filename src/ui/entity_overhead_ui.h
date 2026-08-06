#ifndef ENTITY_OVERHEAD_UI_H
#define ENTITY_OVERHEAD_UI_H

#include <raylib.h>
#include <stdbool.h>
#include <stdint.h>

/* World-space overhead UI above one entity. Draws bottom to top: HP bar,
 * nameplate, capability bar (Σ-stats circle plus capability icons), presence
 * status icon. Read-only — the module never changes game state. */

/* ── Layout constants ─────────────────────────────────────────────────── */

/* All overhead geometry below is in FIXED screen pixels — uniform for every
 * entity, independent of its world size (only the vertical anchor above the
 * entity tracks world space). The three rows (HP bar, nameplate, capability
 * bar) share one pill background, height, padding, and rounding. */

/* Gap between the entity top edge and the stack, in world units. */
#define EOHUD_GAP_ABOVE_ENTITY  0.12f

/* Shared pill height for all three rows (HP, nameplate, capability bar). */
#define EOHUD_BAR_H             22

#define EOHUD_HP_BAR_W          104
#define EOHUD_PILL_PAD_X        8       /* horizontal padding inside a pill */
#define EOHUD_PILL_ROUND        0.5f    /* corner roundness, 0..1 */
#define EOHUD_ROW_GAP           3       /* between stacked rows */
#define EOHUD_ITEM_GAP          5       /* between capability-bar items */

#define EOHUD_PRESENCE_SIZE     28      /* presence icon, topmost */
#define EOHUD_CAP_ICON_SIZE     20      /* capability-row icons */

#define EOHUD_NAME_FONT_SIZE    13
#define EOHUD_HP_LABEL_FONT_SIZE 12
#define EOHUD_STATS_FONT_SIZE   16
#define EOHUD_RESPAWN_FONT_SIZE 20

/* Death countdown row — taller than EOHUD_BAR_H so its larger font fits. */
#define EOHUD_RESPAWN_BAR_H     30

/* ── Data model ───────────────────────────────────────────────────────── */

/* Fill this from entity state each frame. The module keeps no cache, so the
 * caller owns the lifetime of the data. */
typedef struct {
    const char *name;       /* entity ID or nickname */

    /* Sum of the entity's active stats, capped at sum_stats_limit. Shows in
     * the leading circle of the capability bar. */
    int stats_sum;

    float life;
    float max_life;         /* 0 hides the HP bar */

    bool show_stats;        /* the capability bar row; off for the dead */

    /* Draws the leading Σ-stats icon and value. False suppresses that
     * element only — the capability icons stay. Used for provider NPCs. */
    bool show_stats_value;

    bool show_name;         /* off for projectile bots */
    bool show_hp;           /* off for the dead */

    uint8_t status_icon;    /* presence lifecycle icon; 0 = none */

    /* Per-player capability bitmask (INTERACTION_FLAG_*), 0 for non-bots.
     * Each set bit adds its icon to the capability bar. */
    uint8_t interaction_flags;

    /* Remaining respawn seconds; 0 hides the row. The caller sets this for
     * the self-player only, so no client shows another player's countdown. */
    int respawn_seconds;
} EntityOverheadParams;

/* Draw the stack above one entity. Call inside BeginMode2D / EndMode2D. The
 * world_* rect is the entity footprint in grid units; `cell_size` is pixels
 * per world unit. Stateless — call in any order, for any entity count. */
void entity_overhead_ui_draw(
    const EntityOverheadParams *p,
    float world_x,
    float world_y,
    float world_w,
    float world_h,
    float cell_size
);

#endif /* ENTITY_OVERHEAD_UI_H */
