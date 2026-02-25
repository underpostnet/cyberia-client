/**
 * @file services.js
 * @brief JavaScript service layer for HTTP requests from WebAssembly
 *
 * This file provides JavaScript-based fetch functions that can be called
 * from C/C++ code via Emscripten. Uses the Cyberia engine API
 * for atlas sprite sheet metadata, file blob retrieval, and object layer metadata.
 *
 * Public API endpoints (no auth required):
 *   - GET {api_base_url}/api/object-layer/?filterModel=...&limit=1
 *   - GET {api_base_url}/api/atlas-sprite-sheet/?filterModel=...&limit=1
 *   - GET {api_base_url}/api/file/blob/{fileId}
 *
 * All requests use simple GET without credentials or custom auth headers,
 * avoiding CORS preflight OPTIONS requests entirely.
 */

mergeInto(LibraryManager.library, {
  // ==========================================================================
  // Internal State
  // ==========================================================================

  $FetchState: {
    // Engine API base URL (set from C config via js_init_engine_api)
    api_base_url: "https://www.cyberiaonline.com",

    // Async fetch tracking
    active: {},
    completed: {},
  },

  // ==========================================================================
  // Library Functions
  // ==========================================================================

  /**
   * @brief Initialize engine API base URL
   *
   * Called from C code during startup. Sets the engine API base URL
   * used by all subsequent fetch calls.
   *
   * @param api_base_url_ptr Pointer to C string with engine API base URL
   */
  js_init_engine_api__deps: ["$FetchState"],
  js_init_engine_api: function (api_base_url_ptr) {
    var api_base_url = UTF8ToString(api_base_url_ptr);

    if (api_base_url) {
      FetchState.api_base_url = api_base_url;
    }

    console.log("[API] Engine API base URL set to: " + FetchState.api_base_url);
  },

  /**
   * @brief Build minimal headers for API requests
   *
   * Returns headers that qualify as a CORS "simple request"
   * (no preflight OPTIONS needed).
   *
   * @returns {Object} Headers object
   */
  $buildHeaders: function () {
    return {
      Accept: "application/json",
    };
  },

  /**
   * @brief Fetch atlas sprite sheet data from the engine API
   *
   * Fetches AtlasSpriteSheet metadata by item key from the engine API.
   * Uses the DataQuery filterModel to filter by metadata.itemKey.
   *
   * API: GET {api_base_url}/api/atlas-sprite-sheet/?filterModel=...&limit=1
   *
   * Response structure:
   * {
   *   "status": "success",
   *   "data": {
   *     "data": [{
   *       "_id": "...",
   *       "fileId": { "_id": "hexstring", ... },
   *       "metadata": {
   *         "itemKey": "anon",
   *         "atlasWidth": 400,
   *         "atlasHeight": 800,
   *         "cellPixelDim": 20,
   *         "frames": { "down_idle": [{x,y,width,height,frameIndex},...], ... }
   *       }
   *     }],
   *     "total": 1
   *   }
   * }
   *
   * @param item_key_ptr Pointer to C string containing the item key
   * @return Pointer to allocated string containing JSON response, or NULL on error
   */
  js_fetch_atlas_sprite_sheet__deps: ["$FetchState", "$buildHeaders"],
  js_fetch_atlas_sprite_sheet: function (item_key_ptr) {
    var item_key = UTF8ToString(item_key_ptr);

    var filterModel = JSON.stringify({
      "metadata.itemKey": {
        filterType: "text",
        type: "equals",
        filter: item_key,
      },
    });

    var url =
      FetchState.api_base_url +
      "/api/atlas-sprite-sheet/?filterModel=" +
      encodeURIComponent(filterModel) +
      "&limit=1";

    return Asyncify.handleAsync(function () {
      return fetch(url, {
        method: "GET",
        headers: buildHeaders(),
      })
        .then(function (response) {
          if (!response.ok) {
            console.error(
              "[FETCH ERROR] Atlas sprite sheet API returned " +
                response.status +
                " for item: " +
                item_key,
            );
            return 0;
          }
          return response.text().then(function (data) {
            var buffer_size = lengthBytesUTF8(data) + 1;
            var buffer = _malloc(buffer_size);

            if (!buffer) {
              console.error(
                "[FETCH ERROR] Memory allocation failed for atlas sprite sheet response",
              );
              return 0;
            }

            stringToUTF8(data, buffer, buffer_size);
            return buffer;
          });
        })
        .catch(function (error) {
          console.error(
            "[FETCH ERROR] Failed to fetch atlas sprite sheet for " +
              item_key +
              ": " +
              error.message,
          );
          return 0;
        });
    });
  },

  /**
   * @brief Fetch object layer metadata from the engine API
   *
   * Fetches ObjectLayer data by item ID using the DataQuery filterModel.
   * This provides item type, stats, frame_duration, is_stateless, etc.
   *
   * API: GET {api_base_url}/api/object-layer/?filterModel=...&limit=1
   *
   * @param item_id_ptr Pointer to C string containing the item ID
   * @return Pointer to allocated string containing JSON response, or NULL on error
   */
  js_fetch_object_layer__deps: ["$FetchState", "$buildHeaders"],
  js_fetch_object_layer: function (item_id_ptr) {
    var item_id = UTF8ToString(item_id_ptr);

    var filterModel = JSON.stringify({
      "data.item.id": {
        filterType: "text",
        type: "equals",
        filter: item_id,
      },
    });

    var url =
      FetchState.api_base_url +
      "/api/object-layer/?filterModel=" +
      encodeURIComponent(filterModel) +
      "&limit=1";

    return Asyncify.handleAsync(function () {
      return fetch(url, {
        method: "GET",
        headers: buildHeaders(),
      })
        .then(function (response) {
          if (!response.ok) {
            console.error(
              "[FETCH ERROR] Object layer API returned " +
                response.status +
                " for item: " +
                item_id,
            );
            return 0;
          }
          return response.text().then(function (data) {
            var buffer_size = lengthBytesUTF8(data) + 1;
            var buffer = _malloc(buffer_size);

            if (!buffer) {
              console.error(
                "[FETCH ERROR] Memory allocation failed for object layer response",
              );
              return 0;
            }

            stringToUTF8(data, buffer, buffer_size);
            return buffer;
          });
        })
        .catch(function (error) {
          console.error(
            "[FETCH ERROR] Failed to fetch object layer for " +
              item_id +
              ": " +
              error.message,
          );
          return 0;
        });
    });
  },

  /**
   * @brief Start an asynchronous binary fetch (non-blocking)
   *
   * Used to fetch atlas PNG blobs from the file API:
   *   GET {api_base_url}/api/file/blob/{fileId}
   *
   * The URL is constructed on the C side and passed in full.
   * No credentials or auth headers are sent for binary blob
   * requests to avoid CORS preflight.
   *
   * @param url_ptr Pointer to URL string
   * @param request_id Unique ID for this request
   */
  js_start_fetch_binary__deps: ["$FetchState"],
  js_start_fetch_binary: function (url_ptr, request_id) {
    var url = UTF8ToString(url_ptr);

    // Prevent duplicate requests
    if (FetchState.active[request_id] || FetchState.completed[request_id]) {
      return;
    }

    FetchState.active[request_id] = true;

    // Use minimal headers for binary fetch — no auth, no credentials
    // This ensures the request is a CORS "simple request" (no preflight)
    fetch(url, {
      method: "GET",
    })
      .then(function (response) {
        if (!response.ok) throw new Error("HTTP " + response.status);
        return response.arrayBuffer();
      })
      .then(function (buffer) {
        var byteArray = new Uint8Array(buffer);
        FetchState.completed[request_id] = {
          data: byteArray,
          status: 1, // Success
        };
        delete FetchState.active[request_id];
      })
      .catch(function (err) {
        console.error(
          "[FETCH ERROR] Async binary fetch failed for " + url + ": ",
          err,
        );
        FetchState.completed[request_id] = {
          data: null,
          status: -1, // Error
        };
        delete FetchState.active[request_id];
      });
  },

  /**
   * @brief Check status of an async fetch
   *
   * @param request_id Request ID to check
   * @param size_ptr Pointer to write data size if successful
   * @return Pointer to data (needs freeing) if ready, 0 if pending/error
   *         If return is 0, check size_ptr: 0=pending, -1=error
   */
  js_get_fetch_result__deps: ["$FetchState"],
  js_get_fetch_result: function (request_id, size_ptr) {
    var result = FetchState.completed[request_id];

    if (!result) {
      if (FetchState.active[request_id]) {
        HEAP32[size_ptr >> 2] = 0; // Pending
      } else {
        HEAP32[size_ptr >> 2] = -1; // Unknown ID
      }
      return 0;
    }

    if (result.status === -1) {
      HEAP32[size_ptr >> 2] = -1; // Error
      delete FetchState.completed[request_id];
      return 0;
    }

    // Success — allocate C memory and copy
    var byteArray = result.data;
    var size = byteArray.length;
    var buffer = _malloc(size);

    if (!buffer) {
      console.error("[FETCH ERROR] Memory allocation failed for async result");
      HEAP32[size_ptr >> 2] = -1;
      return 0;
    }

    HEAPU8.set(byteArray, buffer);
    HEAP32[size_ptr >> 2] = size;

    // Clean up JS side
    delete FetchState.completed[request_id];

    return buffer;
  },
});
