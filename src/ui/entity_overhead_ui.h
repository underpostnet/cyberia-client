/**
 * @file entity_overhead_ui.h
 * @brief World-space overhead UI rendered above each game entity.
 *
 * Rendering stack (drawn bottom → top, above the entity), in world space
 * (inside BeginMode2D / EndMode2D):
 *
 *   [presence status icon]                  ← lifecycle icon (unchanged)
 *   ( Σ )[action][quest]   capability bar    ← Σ-stats circle + capability icons
 *   [nameplate text]                         ← display name label
 *   [====HP  bar======]    health bar        ← life / max_life
 *            │
 *         entity
 *
 * All values are read-only; the module never mutates game state.
 */

#ifndef ENTITY_OVERHEAD_UI_H
#define ENTITY_OVERHEAD_UI_H

#include <raylib.h>
#include <stdbool.h>
#include <stdint.h>

/* ── Layout constants (world-space, scaled by cell_size at draw time) ─── */

/* All overhead geometry below is in FIXED screen pixels — uniform for every
 * entity, independent of its world size (only the vertical anchor above the
 * entity tracks world space). The three rows (HP bar, nameplate, capability
 * bar) share one pill background, height, padding, and rounding. */

/** Vertical gap between the entity top edge and the overhead stack (world). */
#define EOHUD_GAP_ABOVE_ENTITY  0.12f

/** Shared pill height for all three rows (HP, nameplate, capability bar). */
#define EOHUD_BAR_H             22

/** HP bar width. */
#define EOHUD_HP_BAR_W          104

/** Horizontal padding inside every pill. */
#define EOHUD_PILL_PAD_X        8

/** Pill corner roundness (0..1) and vertical gap between stacked rows. */
#define EOHUD_PILL_ROUND        0.5f
#define EOHUD_ROW_GAP           3

/** Horizontal gap between items in the capability bar. */
#define EOHUD_ITEM_GAP          5

/** Uniform icon sizes: presence (standalone, topmost), capability icon
 *  (slightly smaller), and the capability-row icons. */
#define EOHUD_PRESENCE_SIZE     28
#define EOHUD_CAP_ICON_SIZE     20

/** Label font sizes (screen pixels). The sum-of-stats value is the largest. */
#define EOHUD_NAME_FONT_SIZE    13
#define EOHUD_HP_LABEL_FONT_SIZE 12
#define EOHUD_STATS_FONT_SIZE   16

/* ── Data model ─────────────────────────────────────────────────────────── */

/**
 * @struct EntityOverheadParams
 * @brief All inputs needed to render the overhead UI for one entity.
 *
 * Fill this struct each frame from entity state; the module is stateless and
 * performs no caching — the caller owns the data lifetime.
 */
typedef struct {
    /** Display label (e.g. entity ID or human-readable nickname). */
    const char *name;

    /** Sum of the entity's active stats (capped at sum_stats_limit); shown in
     *  the capability bar's leading circle. */
    int stats_sum;

    /** Current life (HP). */
    float life;

    /** Maximum life. Set to 0 to hide the HP bar entirely. */
    float max_life;

    /** Whether to render the capability bar row (Σ-stats value + capability
     *  icons). Suppressed for dead entities. */
    bool show_stats;

    /** Whether the capability bar's leading Σ-stats icon + value is drawn. False
     *  suppresses only that element (capability icons still show) — used for
     *  provider NPCs. */
    bool show_stats_value;

    /** Whether to render the nameplate. Suppressed for projectile bots. */
    bool show_name;

    /** Whether to render the HP bar. Suppressed for dead entities. */
    bool show_hp;

    /** Presence lifecycle icon ID (u8); 0 = none. Rendered unchanged as the
     *  topmost icon. */
    uint8_t status_icon;

    /** Per-player interaction capability bitmask (INTERACTION_FLAG_*), 0 for
     *  non-bots. Each set bit adds its icon to the capability bar. */
    uint8_t interaction_flags;

    /** Remaining respawn seconds for the LOCAL player only; 0 hides the row.
     *  Renders a countdown row above the nameplate and below the presence icon.
     *  The caller sets this exclusively for the self-player so a remote client
     *  never sees another player's respawn countdown. */
    int respawn_seconds;
} EntityOverheadParams;

/* ── Public API ─────────────────────────────────────────────────────────── */

/**
 * @brief Draw the overhead UI stack above a single entity.
 *
 * Must be called inside a BeginMode2D / EndMode2D block.
 * The function is stateless — safe to call in any order, for any number of
 * entities per frame.
 *
 * @param p          Filled EntityOverheadParams (all inputs).
 * @param world_x    Entity left edge in world (grid) coordinates.
 * @param world_y    Entity top edge in world (grid) coordinates.
 * @param world_w    Entity width in world coordinates.
 * @param world_h    Entity height in world coordinates.
 * @param cell_size  Pixels per world unit (from g_game_state.cell_size).
 */
void entity_overhead_ui_draw(
    const EntityOverheadParams *p,
    float world_x,
    float world_y,
    float world_w,
    float world_h,
    float cell_size
);

#endif /* ENTITY_OVERHEAD_UI_H */
