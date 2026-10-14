#include "telemetry.h"

#include "data/engine_client.h"
#include "sim/game_state.h"
#include "sim/local_player.h"
#include "view/entity_render.h"
#include "world/world_types.h"
#include "utils.h"

#include <string.h>

static char s_trace_map[MAX_ID_LENGTH];
static bool s_trace_player;
static bool s_trace_frame;
static bool s_trace_scene;
static bool s_trace_movement;
static bool s_destination;
static Vector2 s_trace_position;

void telemetry_event(const char* event) {
    fetch_event(event, s_trace_map, 0, 0);
}

void telemetry_observe_scene(bool gameplay) {
    if ('\0' == g_local_player.map_code[0]) return;
    const Vector2 pos = game_state_transform(g_local_player.id)->pos;
    if (0 != strcmp(s_trace_map, g_local_player.map_code)) {
        s_destination = '\0' != s_trace_map[0];
        copy_str(s_trace_map, sizeof(s_trace_map), g_local_player.map_code);
        s_trace_player = s_trace_frame = s_trace_movement = s_trace_scene = false;
        s_trace_position = pos;
    }
    if (!s_trace_player && player_render_ready()) {
        s_trace_player = true;
        telemetry_event(s_destination ? "destination_player_ready" : "main_player_ready");
    }
    if (!s_trace_scene && s_trace_player && immediate_scene_ready()) {
        s_trace_scene = true;
        telemetry_event(s_destination ? "destination_activation" : "initial_scene_ready");
    }
    if (gameplay && !s_trace_frame) {
        s_trace_frame = true;
        telemetry_event(s_destination ? "first_destination_frame" : "first_frame");
    }
    if (gameplay && !s_trace_movement &&
        (s_trace_position.x != pos.x || s_trace_position.y != pos.y)) {
        s_trace_movement = true;
        telemetry_event(s_destination ? "first_destination_movement" : "first_movement");
    }
}
