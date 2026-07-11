#include "loading_bridge.h"

#include <emscripten/emscripten.h>

void loading_bridge_progress(float pct, const char* label) {
    EM_ASM(
        {
            if (window.CyberiaLoading) {
                CyberiaLoading.progress($0, $1 ? UTF8ToString($1) : null);
            }
        },
        pct, label);
}

void loading_bridge_ready(void) {
    EM_ASM({
        if (window.CyberiaLoading) CyberiaLoading.setReady();
    });
}

bool loading_bridge_start_requested(void) {
    return 0 != EM_ASM_INT({
               return window.CyberiaLoading ? CyberiaLoading.startRequested()
                                            : 1;
           });
}

void loading_bridge_hide(void) {
    EM_ASM({
        if (window.CyberiaLoading) CyberiaLoading.hide();
    });
}
