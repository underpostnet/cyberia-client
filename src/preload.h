#ifndef PRELOAD_H
#define PRELOAD_H

#include <stdbool.h>

/* Boot phase before gameplay. Order: server connection, then assets.
 * The splash is drawn with raylib; shell.html adds only the CSS effects. */

/* Opens the server connection and loads the splash font. */
void preload_start(void);

/* One full splash frame. True on the frame the player taps to start, after
 * the camera and audio are set for gameplay. */
bool preload_frame(void);

#endif // PRELOAD_H
