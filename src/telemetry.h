#ifndef TELEMETRY_H
#define TELEMETRY_H

#include <stdbool.h>

/* Boot and scene milestones for the profile trace. fetch_event is the sink;
 * it does nothing unless the profile flag is set. */

/* Call once per frame. `gameplay` is false while the splash shows. */
void telemetry_observe_scene(bool gameplay);

/* One milestone, tagged with the current map. */
void telemetry_event(const char* event);

#endif // TELEMETRY_H
