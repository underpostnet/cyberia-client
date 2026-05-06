/**
 * @file services.js
 * @brief Channel 3 — C/WASM Client → Engine HTTP fetch bridge (non-authoritative data)
 *
 * Provides the JS-side of the non-blocking binary fetch bridge used by all
 * direct Engine REST calls from C code.  No auth headers, no credentials —
 * all endpoints here serve publicly visible, read-only reference data.
 *
 * Engine REST endpoints fetched through this bridge (Channel 3):
 *
 *   Atlas sprite sheets (object_layers_management.c):
 *     GET {api_base_url}/api/atlas-sprite-sheet/metadata/{itemKey}
 *         → frame layout JSON (sprite sheet metadata)
 *     GET {api_base_url}/api/atlas-sprite-sheet/blob/{itemKey}
 *         → atlas PNG binary
 *
 *   Dialogue / lore (dialogue_data.c):
 *     GET {api_base_url}/api/cyberia-dialogue/code/default-{itemId}
 *         → dialogue lines JSON for an NPC item ID
 *
 *   UI icons (ui_icon.c):
 *     GET {api_base_url}/assets/ui-icons/{iconId}.png
 *         → status-bar icon PNG
 *
 *   OL stack preview (interact_overlay.js — DOM <img> src, not this bridge):
 *     GET {api_base_url}/assets/{type}/{itemId}/08/0.png
 *
 * ObjectLayer and AtlasSpriteSheet *metadata* for entities already in AOI are
 * delivered via WebSocket from the Go game server (Channel 2) — no REST call.
 *
 * All requests are CORS simple GETs (no preflight) and can be freely cached
 * by the browser or a CDN — the Engine is not authoritative for game state.
 *
 * See: ARCHITECTURE.md § "Channel 3" for the full design rationale.
 */

mergeInto(LibraryManager.library, {
  // ==========================================================================
  // Internal State
  // ==========================================================================

  $FetchState: {
    // Engine API base URL (set from C config via js_init_engine_api)
    api_base_url: 'https://www.cyberiaonline.com',

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
   * used by all subsequent fetch calls (binary blob fetches).
   *
   * @param api_base_url_ptr Pointer to C string with engine API base URL
   */
  js_init_engine_api__deps: ['$FetchState'],
  js_init_engine_api: function (api_base_url_ptr) {
    var api_base_url = UTF8ToString(api_base_url_ptr);

    if (api_base_url) {
      FetchState.api_base_url = api_base_url;
    }

    console.log('[API] Engine API base URL set to: ' + FetchState.api_base_url);
  },

  /**
   * @brief Start an asynchronous binary fetch (non-blocking)
   *
   * General-purpose non-blocking GET used for all Channel 3 requests:
   * atlas PNG blobs, atlas metadata JSON, dialogue JSON, and UI icon PNGs.
   *
   * The full URL (including API_BASE_URL prefix) is constructed on the C side
   * and passed in. No credentials or auth headers are sent — all responses
   * are public read-only data, keeping every request a CORS simple request
   * with no preflight round-trip.
   *
   * @param url_ptr   Pointer to the full URL C string
   * @param request_id Unique integer ID for polling via js_get_fetch_result
   */
  js_start_fetch_binary__deps: ['$FetchState'],
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
      method: 'GET',
    })
      .then(function (response) {
        if (!response.ok) throw new Error('HTTP ' + response.status);
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
        console.error('[FETCH ERROR] Async binary fetch failed for ' + url + ': ', err);
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
  js_get_fetch_result__deps: ['$FetchState'],
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
      console.error('[FETCH ERROR] Memory allocation failed for async result');
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
