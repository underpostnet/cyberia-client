#ifndef CYBERIA_UI_IDENTITY_ICON_H
#define CYBERIA_UI_IDENTITY_ICON_H

#include <raylib.h>
#include <stddef.h>

#include "world/object_layer.h"

/* identity_icon — the icon that stands for a player in social views: the
 * identity preview of the visual layers it wears, a PNG the engine draws. One
 * table keeps the latest reference of each player name, from chat lines,
 * friend rows and profiles. A player keeps its last drawn image while a newer
 * one loads; the anon icon stands in until one draws. */

/* The reference of the active visual layers, each on the definition the world
 * binds its label to; "" for none, or for more than the contract allows. */
void identity_appearance_ref(const ObjectLayerState* layers, int count, char* out, size_t size);

/* Takes the latest reference of a player; "" keeps the one it has. */
void identity_icon_note(const char* name, const char* avatar);

void identity_icon_draw(const char* name, Rectangle r);
void identity_preview_draw(const char* name, const char* note, Rectangle r, int font, Color color, Color note_color);

#endif /* CYBERIA_UI_IDENTITY_ICON_H */
