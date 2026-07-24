#ifndef CYBERIA_UI_BUTTON_PIXEL_RETRO_H
#define CYBERIA_UI_BUTTON_PIXEL_RETRO_H

#include <raylib.h>
#include <stdbool.h>

/* ui_button_pixel_retro — shared pixel-art button chrome.
 *
 * One drawing path for every retro-styled tap target: a black outer border, a
 * flat fill in the caller's `bg` colour, a lighter top edge and darker bottom
 * edge derived from that colour, an optional left icon, and a label with a
 * black outline for legibility. A white inner outline marks hover/selected.
 *
 * Two label layouts:
 *   - centered group  (wrap_label = false): icon + single-line label centered
 *     as a group. Content-fit buttons (Accept/Abandon, Activate, Dialog).
 *   - left icon + wrap (wrap_label = true): icon pinned left, label wrapped in
 *     the remaining width. Wide list buttons (quest-talk switcher).
 */

typedef struct {
    Color       bg;         /* main fill; hover brightens it, disabled mutes it */
    const char* icon_id;    /* ui-icon stem; NULL/"" = no icon                  */
    const char* label;      /* NULL/"" = no label                              */
    int         font_size;
    Color       text_color; /* alpha 0 → white                                 */
    bool        selected;   /* white inner outline (also drawn on hover)        */
    bool        enabled;    /* false → no hover response, caller mutes bg       */
    bool        wrap_label; /* true → left icon + wrapped label; else centered  */
    bool        no_icon_shadow; /* skip the black icon drop-shadow (icon-only
                                 * buttons: toggle arrows, toolbar)             */
} UIButtonPixelRetroStyle;

void ui_button_pixel_retro_draw(Rectangle bounds, const UIButtonPixelRetroStyle* style, bool hovered);

#endif /* CYBERIA_UI_BUTTON_PIXEL_RETRO_H */
