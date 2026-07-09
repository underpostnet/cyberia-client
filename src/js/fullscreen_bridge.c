#include "fullscreen_bridge.h"

#include <emscripten/emscripten.h>

bool fullscreen_bridge_is_active(void) {
    return 0 != EM_ASM_INT({ return document.fullscreenElement ? 1 : 0; });
}

void fullscreen_bridge_toggle(void) {
    EM_ASM({
        if (document.fullscreenElement) {
            document.exitFullscreen();
        } else {
            document.documentElement.requestFullscreen();
        }
    });
}
