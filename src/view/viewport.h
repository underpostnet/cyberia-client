#ifndef CYBERIA_DOMAIN_VIEWPORT_H
#define CYBERIA_DOMAIN_VIEWPORT_H

#include <stdbool.h>

/* Single source of truth for "is this a small/mobile screen" — keyed off the
 * live window dimensions, not a device/UA sniff. Any module
 * that needs to branch on viewport size (font scaling, camera zoom, layout)
 * should call viewport_is_mobile() instead of comparing screen dimensions
 * against its own hardcoded breakpoint. */

/* Either screen dimension (px) below which the viewport is treated as mobile. */
#define VIEWPORT_MOBILE_BREAKPOINT_PX 600

/* True when the current width or height is below VIEWPORT_MOBILE_BREAKPOINT_PX. */
bool viewport_is_mobile(void);

#endif /* CYBERIA_DOMAIN_VIEWPORT_H */
