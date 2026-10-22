#ifndef CYBERIA_UI_MAP_MARKER_H
#define CYBERIA_UI_MAP_MARKER_H

#include "data/instance_map_data.h"

#include <math.h>
#include <raylib.h>
#include <stdbool.h>

/* map_marker — the marks the minimap and the Instance Map share: the own
 * player, the entry spawn, a bot or a resource, a portal gate, an action pin,
 * a quest badge, a talk badge, a quest target, a direction arrow, the pulse
 * that runs a link and a label. Each shape is unique to its role, so a mark
 * reads without its colour. Icons keep their own colours; `alpha` (0..1) only
 * fades a mark. */

/* The own player on every map. No other mark uses it. */
extern const Color MAP_MARKER_PLAYER;

/* The entry spawn on every map. No other mark uses it. */
extern const Color MAP_MARKER_SPAWN;

/* A route between maps or between worlds. No other link uses it. */
extern const Color MAP_MARKER_ROUTE;

/* Screen sizes of the marks, px. On the Instance Map a mark grows with the
 * screen size of one map cell: its minimum up to a cell of MAP_MARKER_CELL_LOW,
 * its maximum from a cell of MAP_MARKER_CELL_HIGH. Zoom moves a mark and never
 * grows it past its maximum. The minimap draws each mark at its minimum. */
#define MAP_MARKER_CELL_LOW      2.0f
#define MAP_MARKER_CELL_HIGH    12.0f
#define MAP_MARKER_PRESENCE_MIN  7.0f   /* side of a bot or a resource */
#define MAP_MARKER_PRESENCE_MAX 13.0f
#define MAP_MARKER_GATE_MIN      7.0f   /* radius of a portal gate     */
#define MAP_MARKER_GATE_MAX     12.0f
#define MAP_MARKER_PIN_MIN      16.0f   /* side of an action pin plate */
#define MAP_MARKER_PIN_MAX      28.0f
#define MAP_MARKER_QUEST_MIN    20.0f   /* side of a quest or a talk badge */
#define MAP_MARKER_QUEST_MAX    28.0f
#define MAP_MARKER_SPAWN_MIN    18.0f   /* side of the entry spawn     */
#define MAP_MARKER_SPAWN_MAX    26.0f
#define MAP_MARKER_PLAYER_MIN_R  7.0f   /* radius of the player disc   */
#define MAP_MARKER_PLAYER_MAX_R 12.0f

/* The size of a mark between `min` and `max` for a map cell of `cell` screen px. */
static inline float map_marker_size(float cell, float min, float max) {
    float t = (cell - MAP_MARKER_CELL_LOW) / (MAP_MARKER_CELL_HIGH - MAP_MARKER_CELL_LOW);
    return min + (max - min) * fminf(1.0f, fmaxf(0.0f, t));
}

/* A filled triangle with a dark outline, its base at `at`, `size` long along unit `dir`. */
void map_marker_draw_arrow(Vector2 at, Vector2 dir, float size, Color color, float alpha);

/* A portal gate: a diamond of `radius` with a glow, a dark outline and a white core. */
void map_marker_draw_gate(Vector2 at, float radius, Color color, float alpha);

/* A bot or a resource: a passive bot a circle, a hostile bot a triangle, a
 * resource a square, any other presence a small dot; each `side` px wide with a
 * dark outline. `count` over 1 shows as a badge: marks grouped at this zoom. */
void map_marker_draw_presence(Vector2 at, float side, ImapPresenceStatus status, int count, float alpha);

/* The entry spawn: a green hexagon `side` px high with a ring and the home icon. */
void map_marker_draw_spawn(Vector2 at, float side, float alpha);

/* A small round pulse that travels a link. */
void map_marker_draw_pulse(Vector2 at, Color color, float alpha);

/* The own player: a red disc with a white rim, a pulse ring and an arrow
 * along unit `facing`. `radius` is at least MAP_MARKER_PLAYER_MIN_R. */
void map_marker_draw_player(Vector2 at, float radius, Vector2 facing, float alpha, double time);

/* The plate of an action pin of `side` px whose tail points at `cell`. */
Rectangle map_marker_pin_rect(Vector2 cell, float side);

/* An action pin: a dark plate with `icon` and a tail to `cell`. `count`
 * over 1 shows as a badge: the services the action configures. */
void map_marker_draw_pin(Vector2 cell, float side, const char* icon, int count, Color accent, float alpha);

/* A quest badge in `box`: the gold frame of a quest with `icon`, muted when
 * `muted`, and a count badge for more than one quest. */
void map_marker_draw_quest(Rectangle box, const char* icon, bool muted, int count, float alpha);

/* A talk badge in `box`: a round gold-rimmed plate with the chat icon. It marks
 * an NPC an active `talk` objective sends the player to. */
void map_marker_draw_talk(Rectangle box, float alpha);

/* A quest target: gold corner brackets around the mark in `box`, never over
 * it. A followed target adds a slow halo. */
void map_marker_draw_target(Rectangle box, bool followed, float alpha, double time);

/* A 2 px frame around `box`: the selection or the hover of a mark. */
void map_marker_draw_focus(Rectangle box, Color color, float alpha);

/* The size of a label: an opaque box with the text, an arrow before it when
 * `arrow`, and a stat sum after it when `value` is 0 or more. */
Vector2 map_marker_label_size(const char* text, int font, bool arrow, int value);

/* A label in `box`. A strong label has a full border and white text. A stat
 * sum shows with the stack icon, as over an entity in the world. */
void map_marker_draw_label(Rectangle box, const char* text, int font, Color color, bool arrow, bool strong, int value,
                           float alpha);

/* The first side of `anchor` — below, above, right, left — where a box of
 * `size`, `gap` px away and kept inside `bounds`, meets none of `taken`.
 * False when every side is taken. */
bool map_marker_place_label(Vector2 anchor, Vector2 size, float gap, Rectangle bounds, const Rectangle* taken,
                            int count, Rectangle* out);

#endif /* CYBERIA_UI_MAP_MARKER_H */
