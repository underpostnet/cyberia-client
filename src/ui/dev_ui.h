#ifndef DEV_UI_H
#define DEV_UI_H

/* Development overlay: FPS, network throughput, player and game-state
 * readouts. The dev_ui flag in the presentation runtime shows and hides it.
 * The state lives in dev_ui.c — no caller outside the module touches it. */

void dev_ui_init(void);
void dev_ui_cleanup(void);

/* Recompute the throughput figures. Call once per frame. */
void dev_ui_on_tick(void);

void dev_ui_draw(int screen_width, int screen_height);

#endif // DEV_UI_H
