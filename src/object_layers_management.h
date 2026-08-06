#ifndef OBJECT_LAYERS_MANAGEMENT_H
#define OBJECT_LAYERS_MANAGEMENT_H

#include "object_layer.h"
#include <raylib.h>
#include <cJSON.h>
#include <stddef.h>
#include <stdbool.h>

/* Object-layer cache: atlas frame metadata, atlas GPU textures, and
 * ObjectLayer documents, all keyed by item id.
 *
 * The first request for an item fetches the atlas metadata, then the atlas
 * PNG blob, then uploads one GPU texture. Later requests hit the cache. All
 * endpoints are public GET requests. Cache ceilings come from config.h
 * (MAX_LAYER_CACHE_SIZE, MAX_ATLAS_CACHE_SIZE). */

typedef struct ObjectLayersManager ObjectLayersManager;

void create_object_layers_manager(void);
void destroy_object_layers_manager(void);
ObjectLayersManager* obj_layers_mgr_get(void);

ObjectLayer* lookup_cached_layer(const char* item_id);

/* Atlas data for an item key, fetched on a cache miss. NULL on failure. A
 * miss costs two requests and a PNG decode. */
AtlasSpriteSheetData* get_or_fetch_atlas_data(const char* item_key);

/* Cached atlas texture for an item key. An empty texture (id 0) means the
 * atlas is absent or still loading. */
Texture2D get_atlas_texture(const char* item_key);

/* Parse and cache one ObjectLayer from the WS metadata message. The JSON has
 * the shape { "sha256": ..., "data": { "stats": ..., "item": ..., ... } };
 * the caller keeps ownership. */
void populate_object_layer_from_json(const char* item_id, const cJSON* ol_json);

/* Schedule the atlas metadata fetch for an item key. Once the metadata
 * lands, the next get_atlas_texture call fetches the PNG blob. Repeated
 * calls for the same key are no-ops. */
void obj_layers_mgr_schedule_atlas_fetch(const char* item_key);

#endif // OBJECT_LAYERS_MANAGEMENT_H
