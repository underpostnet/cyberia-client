#ifndef CYBERIA_JS_LOADING_BRIDGE_H
#define CYBERIA_JS_LOADING_BRIDGE_H

#include <stdbool.h>

/* Bridge to the DOM loading screen (window.CyberiaLoading in shell.html).
 *
 * The overlay is the production loading UI: it stays above the canvas from
 * page load until every REAL initialization stage has completed and the
 * player explicitly taps to start. The C client reports stage completions
 * through this bridge — the overlay itself has no timers and no simulated
 * progress. */

/* Report live progress: `pct` is 0..100 from real stage/fetch accounting
 * (the overlay clamps it monotonic) and `label` names the stage or asset
 * currently loading (NULL keeps the current text). */
void loading_bridge_progress(float pct, const char* label);

/* All stages done: stop the progress state and show "TAP TO START". */
void loading_bridge_ready(void);

/* True once the player tapped/keyed the ready overlay. */
bool loading_bridge_start_requested(void);

/* Fade the overlay out and remove it from the DOM. */
void loading_bridge_hide(void);

#endif /* CYBERIA_JS_LOADING_BRIDGE_H */
