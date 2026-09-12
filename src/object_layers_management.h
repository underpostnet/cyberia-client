#ifndef OBJECT_LAYERS_MANAGEMENT_H
#define OBJECT_LAYERS_MANAGEMENT_H

#include "network/data/engine_client.h"
#include "object_layer.h"
#include <raylib.h>
#include <cJSON.h>
#include <stddef.h>
#include <stdbool.h>

// This manager owns parsed content. The request table owns textures.
typedef struct ObjectLayersManager ObjectLayersManager;

void create_object_layers_manager(void);
void destroy_object_layers_manager(void);
ObjectLayersManager* obj_layers_mgr_get(void);

ObjectLayer* lookup_cached_layer(const char* item_id);

/* Atlas data for an item key, fetched on a cache miss. NULL on failure. A
 * miss costs two requests and a PNG decode. */
AtlasSpriteSheetData* get_or_fetch_atlas_data(const char* item_key, FetchPriority priority);

/* Cached atlas texture for an item key. An empty texture (id 0) means the
 * atlas is absent or still loading. */
Texture2D get_atlas_texture(const char* item_key, FetchPriority priority);

/* Parse and cache one ObjectLayer from the WS metadata message. The JSON has
 * the shape { "sha256": ..., "data": { "stats": ..., "item": ..., ... } };
 * the caller keeps ownership. */
void populate_object_layer_from_json(const char* item_id, const cJSON* ol_json);

void obj_layers_mgr_schedule_atlas_fetch(const char* item_key, FetchPriority priority);

#endif // OBJECT_LAYERS_MANAGEMENT_H
