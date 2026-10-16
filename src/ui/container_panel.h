#ifndef CYBERIA_UI_CONTAINER_PANEL_H
#define CYBERIA_UI_CONTAINER_PANEL_H

#include <raylib.h>
#include <stdbool.h>

#include "net/client_event.h"
#include "world/object_layer.h"
#include "world/world_types.h"

/* container_panel — the grid of one player container inside a modal's content
 * rect: an NPC vault at its terminal, or the personal storage. Each cell is a
 * slot of the container. Stacks move inside the grid and to and from the
 * inventory bar by drag, and ui/stack_move decides how each drop lands.
 *
 * The vault opens every cell of its authored capacity. The personal storage
 * shows every cell its rule can open and locks the ones the level has not
 * opened yet. One panel is open at a time. */

/* Tap on a filled cell: the host opens its read-only inspection. */
typedef void (*ContainerPanelInspectFn)(const ObjectLayerState* ols);

/* Open the vault `action_code` of `capacity` cells at terminal `terminal_id`,
 * or the personal storage. */
void container_panel_open_vault(EntityID terminal_id, const char* action_code, int capacity,
                                ContainerPanelInspectFn inspect);
void container_panel_open_storage(ContainerPanelInspectFn inspect);

/* Send the pending ops and close. Safe when nothing is open. */
void container_panel_close(void);
bool container_panel_is_open(void);

/* `ready` false holds back the grid's events, for example while the content
 * pops in. */
void container_panel_update(Rectangle content, bool ready, float dt);

/* Draws the panel and returns the height its content needs. */
float container_panel_draw(Rectangle content);

bool container_panel_press(Rectangle content, int mx, int my);
bool container_panel_wheel(Rectangle content, float wheel_delta);

/* The dragged item, drawn after the inventory bar so it rides above it. */
void container_panel_draw_drag(void);

/* The box and slot of the open panel cell under the screen point. False with
 * no panel open or off its cells. */
bool container_panel_cell_at(int mx, int my, Box* box, int* slot);

/* The screen rect of the panel cell of `slot`. */
Rectangle container_panel_cell_rect(int slot);

/* Slide the stack of `slot` in from `origin`, a screen rect. */
void container_panel_animate_from(int slot, Rectangle origin);

#endif /* CYBERIA_UI_CONTAINER_PANEL_H */
