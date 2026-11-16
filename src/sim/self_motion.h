#ifndef CYBERIA_DOMAIN_SELF_MOTION_H
#define CYBERIA_DOMAIN_SELF_MOTION_H

#include <raylib.h>

#include "world/object_layer.h"
#include "world/world_types.h"

/* The motion of the own player. The live step and the replay both move it
 * with self_motion_step, which mirrors the server stepAlongPath. */
typedef struct {
    Vector2   pos;
    Vector2   path[MAX_PATH_POINTS];  /* remaining waypoints, head first */
    int       path_count;
    Direction direction;              /* the facing; a stop keeps it */
} self_motion_t;

/* Walk straight to the cell of target, as the server PendingMove rounds it.
 * A tap on the cell the player stands on stops the walk in place. A walk to
 * that cell keeps its path, as the server keeps it. */
void self_motion_tap(self_motion_t* m, Vector2 target);

/* Drop the path, as FreezePlayer does. */
void self_motion_stop(self_motion_t* m);

/* One tick along the path. The rest of the step after a node walks the next leg. */
void self_motion_step(self_motion_t* m, double speed, double dt);

/* The facing whose CYBERIA_DIRECTION_VECTORS entry has the largest dot
 * product with a vector, as the server directionFromVector. DIRECTION_NONE
 * for a zero vector. */
Direction direction_of(double dx, double dy);

#endif /* CYBERIA_DOMAIN_SELF_MOTION_H */
