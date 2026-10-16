#ifndef CYBERIA_UI_IDENTITY_ICON_H
#define CYBERIA_UI_IDENTITY_ICON_H

#include <raylib.h>

/* identity_icon — the icon that stands for a player in social views: its
 * active skin as an Object Layer icon, framed. The anon icon stands in for a
 * player whose skin is unknown or not loaded yet. */
void identity_icon_draw(const char* skin_item_id, Rectangle r);

#endif /* CYBERIA_UI_IDENTITY_ICON_H */
