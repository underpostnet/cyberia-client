// Transport aura: blue sparks circle the feet of a player that stands on a
// portal and spiral up through its body. The teleport charge raises the column
// and adds sparks, so the aura gathers as the jump nears. It shows on every
// player in view and stops with the hold.
//
// Integration pattern:
//   update:  fx_portal_aura_update(delta_time);
//   draw:    fx_portal_aura_draw();   // inside BeginMode2D, above the entities

#ifndef CYBERIA_FX_FX_PORTAL_AURA_H
#define CYBERIA_FX_FX_PORTAL_AURA_H

void fx_portal_aura_update(float dt);
void fx_portal_aura_draw(void);

#endif /* CYBERIA_FX_FX_PORTAL_AURA_H */
