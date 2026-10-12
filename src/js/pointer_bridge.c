#include "pointer_bridge.h"

#include <emscripten/emscripten.h>

EM_JS(int, pointer_bridge_press_count, (void), {
    if (!Module.cyberiaPointerPresses) {
        Module.cyberiaPointerPresses = { count: 0 };
        Module.canvas.addEventListener('pointerdown', () => Module.cyberiaPointerPresses.count++,
                                       { capture: true, passive: true });
    }
    return Module.cyberiaPointerPresses.count;
})
