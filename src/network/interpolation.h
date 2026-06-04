/**
 * @file network/interpolation.h
 * @brief Render-time interpolation of remote entities.
 *
 * Interpolation runs at render-frame cadence (vsync) and produces
 * view-time positions for entities the local player does not own. The
 * render-tick is computed by session_render_tick() and equals
 * server_tick_estimate - INTERP_TICKS.
 *
 * The decoder writes the latest server position into entity.pos_server
 * and copies the previous one to entity.pos_prev each snapshot. This
 * module lerps between them based on time since the last snapshot.
 *
 * Ownership:
 *   - This module is the only writer of EntityState.interp_pos for remote
 *     entities (other_players and bots).
 *   - It is NEVER called on the local player — prediction owns that.
 *   - The renderer reads entity.interp_pos but never writes it.
 */

#ifndef CYBERIA_INTERPOLATION_H
#define CYBERIA_INTERPOLATION_H

/** Compute view-time positions for all remote entities. Called once per
 *  render frame from app_loop, after network ingestion / reconciliation. */
void interpolation_compute_view(void);

#endif /* CYBERIA_INTERPOLATION_H */
