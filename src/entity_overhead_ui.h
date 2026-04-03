/**
 * @file entity_overhead_ui.h
 * @brief World-space overhead UI rendered above each game entity.
 *
 * Renders three stacked elements anchored above the entity bounding box,
 * in world space (inside BeginMode2D / EndMode2D):
 *
 *   ┌──────────────────────┐   ← nameplate   (display name label)
 *   │  [===level bar=====] │   ← level bar   (effective level / sum_stats_limit)
 *   │  [====HP  bar======] │   ← health bar   (life / max_life)
 *   └──────────────────────┘
 *            │
 *         entity
 *
 * Usage — call once per entity per frame, after the entity sprite is drawn
 * but still inside BeginMode2D:
 *
 *   EntityOverheadParams p = {
 *       .name            = entity_base->id,
 *       .effective_level = entity_base->effective_level,
 *       .max_level       = g_game_state.sum_stats_limit,
 *       .life            = entity_base->life,
 *       .max_life        = entity_base->max_life,
 *       .show_level      = true,
 *   };
 *   entity_overhead_ui_draw(&p,
 *       entity_base->interp_pos.x, entity_base->interp_pos.y,
 *       entity_base->dims.x,       entity_base->dims.y,
 *       cell_size);
 *
 * All values are read-only; the module never mutates game state.
 */

#ifndef ENTITY_OVERHEAD_UI_H
#define ENTITY_OVERHEAD_UI_H

#include <stdbool.h>
#include <raylib.h>

/* ── Layout constants (world-space, scaled by cell_size at draw time) ─── */

/** Vertical gap between the top of the entity and the bottom of the HP bar.  */
#define EOHUD_GAP_ABOVE_ENTITY  0.10f   /* world units */

/** Height of each bar (HP / load) in world units. */
#define EOHUD_BAR_HEIGHT        0.22f

/** Vertical gap between stacked bars (label now sits inside the bar). */
#define EOHUD_BAR_SPACING       0.06f

/** Vertical spacing between load bar and nameplate text. */
#define EOHUD_NAME_SPACING      0.04f

/** Bar width relative to entity width (1.0 = same as entity). */
#define EOHUD_BAR_WIDTH_RATIO   1.10f

/** Font size for the nameplate in screen pixels. Fixed; not world-scaled. */
#define EOHUD_NAME_FONT_SIZE    10

/** Font size for the HP label (e.g. "HP 73/100") in screen pixels. */
#define EOHUD_HP_LABEL_FONT_SIZE   11

/** Font size for the level label (e.g. "Lv. 15 / 30") in screen pixels. */
#define EOHUD_LEVEL_LABEL_FONT_SIZE 11

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

    /**
     * Current effective level (clamped sum of all stat fields).
     * Transmitted by the server for every entity (player and bot).
     */
    int effective_level;

    /**
     * Maximum effective level (sum_stats_limit).
     * Set to 0 to hide the level bar entirely.
     */
    int max_level;

    /** Current life (HP). */
    float life;

    /** Maximum life. Set to 0 to hide the HP bar entirely. */
    float max_life;

    /**
     * Whether to render the effective level bar.
     * Display for all living entities.
     */
    bool show_level;

    /**
     * Whether to render the nameplate.
     * May be suppressed for skill/coin projectile bots.
     */
    bool show_name;

    /**
     * Whether to render the HP bar.
     * May be suppressed for dead entities (respawn_in > 0).
     */
    bool show_hp;
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
