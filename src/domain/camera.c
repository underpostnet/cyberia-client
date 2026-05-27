#include "camera.h"

#include "presentation_runtime.h"
#include "game_state.h"
#include "prediction/prediction.h"
#include "util/log.h"

#include <math.h>
#include <raymath.h>

/* Frame-rate independent exponential-smoothing constant for camera follow.
 * The original constant 0.15 was tuned at 60 fps → lambda ≈ 10. */
#define CAMERA_FOLLOW_LAMBDA 10.0f

static Camera2D g_camera = {
    .offset   = {0.0f, 0.0f},
    .target   = {0.0f, 0.0f},
    .rotation = 0.0f,
    .zoom     = 1.0f,
};

void camera_init(int screen_width, int screen_height) {
    Vector2 self = prediction_self_position();
    float cell = g_game_state.cell_size > 0.0f ? g_game_state.cell_size : 1.0f;
    float cx = (self.x + g_game_state.player.base.dims.x / 2.0f) * cell;
    float cy = (self.y + g_game_state.player.base.dims.y / 2.0f) * cell;

    g_camera.offset   = (Vector2){ screen_width / 2.0f, screen_height / 2.0f };
    g_camera.target   = (Vector2){ cx, cy };
    g_camera.rotation = 0.0f;
    if (g_camera.zoom <= 0.0f) {
        g_camera.zoom = presentation_runtime_camera_zoom();
    }
    LOG_INFO("camera_init target=(%.1f, %.1f) zoom=%.2f", cx, cy, g_camera.zoom);
}

void camera_resize(int screen_width, int screen_height) {
    g_camera.offset.x = screen_width / 2.0f;
    g_camera.offset.y = screen_height / 2.0f;
}

void camera_set_zoom(float zoom) {
    g_camera.zoom = Clamp(zoom, 0.1f, 5.0f);
}

float camera_zoom(void) {
    return g_camera.zoom;
}

void camera_zoom_by(float factor) {
    camera_set_zoom(g_camera.zoom * factor);
}

void camera_update(float frame_dt) {
    float cell = g_game_state.cell_size > 0.0f ? g_game_state.cell_size : 12.0f;
    Vector2 self = g_game_state.player.base.interp_pos;
    float dx = self.x + g_game_state.player.base.dims.x / 2.0f;
    float dy = self.y + g_game_state.player.base.dims.y / 2.0f;
    Vector2 desired = { dx * cell, dy * cell };

    float blend = 1.0f - expf(-CAMERA_FOLLOW_LAMBDA * frame_dt);
    g_camera.target.x += (desired.x - g_camera.target.x) * blend;
    g_camera.target.y += (desired.y - g_camera.target.y) * blend;
}

Camera2D camera_get(void) {
    return g_camera;
}
