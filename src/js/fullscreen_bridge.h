#ifndef CYBERIA_JS_FULLSCREEN_BRIDGE_H
#define CYBERIA_JS_FULLSCREEN_BRIDGE_H

#include <stdbool.h>

/* Browser Fullscreen API bridge (Web only, via inline EM_ASM — no separate
 * --js-library file, no build changes needed). Drives the actual browser
 * document into/out of fullscreen; distinct from raylib's ToggleFullscreen(),
 * which only resizes the canvas and does not reflect or control real
 * browser fullscreen state. */

/* True when the browser document is currently in fullscreen. */
bool fullscreen_bridge_is_active(void);

/* Request fullscreen if not active, or exit fullscreen if active. Must be
 * called from a user-gesture handler (e.g. a click/tap callback) — browsers
 * reject Fullscreen API calls made outside one. */
void fullscreen_bridge_toggle(void);

#endif /* CYBERIA_JS_FULLSCREEN_BRIDGE_H */
