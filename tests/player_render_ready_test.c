#include "entity_render.c"

#include <assert.h>
#include <stdio.h>
#include <string.h>

Camera2D camera_get(void) { return (Camera2D){.zoom = 1.0f}; }
Vector2 GetScreenToWorld2D(Vector2 position, Camera2D camera) { return position; }
int GetScreenWidth(void) { return 800; }
int GetScreenHeight(void) { return 600; }
double GetTime(void) { return 0.0; }
float GetFrameTime(void) { return 0.0f; }
bool CheckCollisionRecs(Rectangle a, Rectangle b) { return true; }
void DrawRectangle(int x, int y, int width, int height, Color color) {}
void DrawRectangleRec(Rectangle rectangle, Color color) {}
void DrawRectangleLinesEx(Rectangle rectangle, float thick, Color color) {}
void DrawTexturePro(Texture2D texture, Rectangle source, Rectangle dest, Vector2 origin, float rotation, Color tint) {}
void text_draw_compat(const char* text, int x, int y, int size, Color color) {}

GameState g_game_state;
ObjectLayerState g_layer_pool[LAYER_POOL_SIZE];
static AtlasSpriteSheetData atlases[4];
static ObjectLayer object_layer;
static bool metadata[4];
static bool textures[4];
static bool documents[4];
static bool requested[4];

AtlasSpriteSheetData* get_or_fetch_atlas_data(const char* id, FetchPriority priority) {
    assert(FETCH_P0 == priority);
    const int index = id[0] - '0';
    requested[index] = true;
    return metadata[index] ? &atlases[index] : NULL;
}
Texture2D get_atlas_texture(const char* id, FetchPriority priority) {
    assert(FETCH_P0 == priority);
    const int index = id[0] - '0';
    requested[index] = true;
    return (Texture2D){.id = textures[index] ? 1 : 0, .width = 25, .height = 25};
}
ObjectLayer* lookup_cached_layer(const char* id) { return documents[id[0] - '0'] ? &object_layer : NULL; }
Direction local_player_view_direction(void) { return DIRECTION_DOWN; }
ObjectLayerMode local_player_view_mode(void) { return MODE_IDLE; }

int main(void) {
    strcpy(g_game_state.player.base.id, "player");
    g_game_state.player.base.layer_count = 2;
    for (int i = 0; 4 > i; i++) {
        g_layer_pool[i].active = true;
        g_layer_pool[i].item_id[0] = '0' + i;
        atlases[i].down_idle.count = 1;
        atlases[i].down_idle.frames[0] = (FrameMetadata){.width = 25, .height = 25};
    }
    assert(!player_render_ready());
    assert(requested[0] && requested[1]);
    metadata[0] = metadata[1] = true;
    documents[0] = documents[1] = true;
    textures[0] = true;
    assert(!player_render_ready());
    textures[1] = true;
    assert(player_render_ready());
    documents[1] = false;
    assert(!player_render_ready());
    documents[1] = true;
    atlases[1].down_idle.frames[0].x = 1;
    assert(!player_render_ready());
    atlases[1].down_idle.frames[0].x = 0;
    atlases[1].down_idle.count = 0;
    assert(!player_render_ready());
    g_layer_pool[1].active = false;
    assert(player_render_ready());
    g_layer_pool[0].active = false;
    assert(!player_render_ready());
    g_layer_pool[0].active = true;

    g_game_state.floor_count = 2;
    g_game_state.floors[0] = (WorldObject){.layer_offset = 2, .layer_count = 1, .dims = {10, 10}};
    g_game_state.floors[1] = (WorldObject){.layer_offset = 3, .layer_count = 1, .pos = {20, 20}, .dims = {10, 10}};
    assert(!immediate_scene_ready());
    assert(requested[2] && !requested[3]);
    metadata[2] = textures[2] = documents[2] = true;
    assert(immediate_scene_ready());
    assert(player_render_ready());
    puts("player readiness tests passed");
}
