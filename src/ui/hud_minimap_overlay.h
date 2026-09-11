#ifndef CYBERIA_UI_HUD_MINIMAP_OVERLAY_H
#define CYBERIA_UI_HUD_MINIMAP_OVERLAY_H

#include <stdbool.h>

/* Full side of the map square; hud_side_stack shortens it when the column is short. */
#define HUD_MINIMAP_OVERLAY_SIZE 300.0f

/* Gameplay HUD whose map content remains click-through; only its close button
 * participates in input dispatch. Its zoom persists across show and hide. */
void hud_minimap_overlay_init(void);
void hud_minimap_overlay_cleanup(void);

void hud_minimap_overlay_show(void);
void hud_minimap_overlay_hide(void);
bool hud_minimap_overlay_is_visible(void);

void hud_minimap_overlay_update(float dt);
void hud_minimap_overlay_draw(void);

/* Consumes only the overlay's close control; all other points pass through. */
bool hud_minimap_overlay_handle_click(int mx, int my);

#endif /* CYBERIA_UI_HUD_MINIMAP_OVERLAY_H */
