#include "viewport.h"

#include <raylib.h>

bool viewport_is_mobile(void) {
    return GetScreenWidth() < VIEWPORT_MOBILE_BREAKPOINT_PX;
}
