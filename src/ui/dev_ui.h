#ifndef DEV_UI_H
#define DEV_UI_H

#include <raylib.h>
#include <stdbool.h>
#include <stddef.h>

/* Development overlay: FPS, network throughput, player and game-state
 * readouts. The dev_ui flag in the presentation runtime shows and hides it. */

typedef struct {
    float download_kbps;
    float upload_kbps;
    double last_network_update;
    size_t last_download_bytes;
    size_t last_upload_bytes;

    int dev_ui_width;
    int dev_ui_height;
    float background_alpha;

    Color background_color;
    Color text_color;
    Color debug_text_color;

    int last_fps;
    double last_fps_update;

    bool show_network_stats;
    bool show_player_stats;
    bool show_game_stats;

} DevUI;

extern DevUI g_dev_ui;

/* Returns 0 on success, -1 on failure. */
int dev_ui_init(void);
void dev_ui_cleanup(void);

/* Recompute the throughput figures. Call once per frame. */
void dev_ui_on_tick(float delta_time);

/* `hud_occupied` is the height the HUD already takes. */
void dev_ui_draw(int screen_width, int screen_height, int hud_occupied);

/* Feed the running byte totals; the module derives the kbps values. */
void dev_ui_update_network_stats(size_t download_bytes, size_t upload_bytes);

/* Both return 0 when the player is absent. */
int dev_ui_get_active_stats_sum(const char* player_id);
int dev_ui_get_active_item_count(const char* player_id);

#endif // DEV_UI_H
