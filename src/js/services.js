/**
 * @file services.js
 * @brief JavaScript service layer for HTTP requests from WebAssembly
 *
 * This file provides JavaScript-based fetch functions that can be called
 * from C/C++ code via Emscripten. Uses the Cyberia engine API
 * (www.cyberiaonline.com) for authentication, atlas sprite sheet metadata,
 * file blob retrieval, and object layer metadata.
 *
 * Authentication flow mirrors cyberia-server/src/object_layer.go:
 *   1. POST {api_base_url}/api/user/auth { email, password }
 *   2. Receive JWT bearer token
 *   3. Use Bearer token in Authorization header for subsequent requests
 */

mergeInto(LibraryManager.library, {
  // ==========================================================================
  // Internal State
  // ==========================================================================

  $FetchState: {
    // Engine API base URL (set from C config via js_init_engine_api)
    api_base_url: "https://www.cyberiaonline.com",

    // JWT bearer token obtained from authentication
    auth_token: "",

    // Whether authentication has been attempted
    auth_attempted: false,

    // Async fetch tracking
    active: {},
    completed: {},
  },

  // ==========================================================================
  // Library Functions
  // ==========================================================================

  /**
   * @brief Initialize engine API connection and authenticate
   *
   * Called from C code during startup. Sets the engine API base URL
   * and authenticates using the provided credentials to obtain a JWT token.
   * If email/password are empty, authentication is skipped (unauthenticated
   * endpoints like atlas-sprite-sheet GET and file blob GET can still be used).
   *
   * Mirrors CyberiaAPIAuthenticate() in cyberia-server/src/object_layer.go
   *
   * @param api_base_url_ptr Pointer to C string with engine API base URL
   * @param email_ptr Pointer to C string with authentication email
   * @param password_ptr Pointer to C string with authentication password
   */
  js_init_engine_api__deps: ["$FetchState"],
  js_init_engine_api: function (api_base_url_ptr, email_ptr, password_ptr) {
    const api_base_url = UTF8ToString(api_base_url_ptr);
    const email = UTF8ToString(email_ptr);
    const password = UTF8ToString(password_ptr);

    if (api_base_url) {
      FetchState.api_base_url = api_base_url;
    }

    // Skip auth if no credentials provided
    if (!email || !password) {
      console.log(
        "[AUTH] No credentials provided, skipping authentication. Unauthenticated endpoints will still work.",
      );
      FetchState.auth_attempted = true;
      return;
    }

    // Use Asyncify for synchronous-like behavior from C
    return Asyncify.handleAsync(async () => {
      try {
        const authUrl = `${FetchState.api_base_url}/api/user/auth`;
        console.log(`[AUTH] Authenticating to ${authUrl}...`);

        const response = await fetch(authUrl, {
          method: "POST",
          headers: {
            "Content-Type": "application/json",
            Accept: "application/json",
          },
          credentials: "include",
          body: JSON.stringify({ email, password }),
        });

        if (!response.ok) {
          console.error(
            `[AUTH] Authentication failed with status ${response.status}`,
          );
          FetchState.auth_attempted = true;
          return;
        }

        const data = await response.json();

        if (data.status === "success" && data.data && data.data.token) {
          FetchState.auth_token = data.data.token;
          console.log("[AUTH] Authentication successful, JWT token obtained.");
        } else {
          console.error("[AUTH] Unexpected auth response format:", data);
        }

        FetchState.auth_attempted = true;
      } catch (error) {
        console.error("[AUTH] Authentication error:", error.message);
        FetchState.auth_attempted = true;
      }
    });
  },

  /**
   * @brief Build common headers for API requests
   * @returns {Object} Headers object with auth token if available
   */
  $buildAuthHeaders__deps: ["$FetchState"],
  $buildAuthHeaders: function () {
    const headers = {
      Accept: "application/json",
    };
    if (FetchState.auth_token) {
      headers["Authorization"] = "Bearer " + FetchState.auth_token;
    }
    return headers;
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
  js_fetch_atlas_sprite_sheet__deps: ["$FetchState", "$buildAuthHeaders"],
  js_fetch_atlas_sprite_sheet: function (item_key_ptr) {
    const item_key = UTF8ToString(item_key_ptr);

    const filterModel = JSON.stringify({
      "metadata.itemKey": {
        filterType: "text",
        type: "equals",
        filter: item_key,
      },
    });

    const url = `${FetchState.api_base_url}/api/atlas-sprite-sheet/?filterModel=${encodeURIComponent(filterModel)}&limit=1`;

    return Asyncify.handleAsync(async () => {
      try {
        const response = await fetch(url, {
          method: "GET",
          headers: buildAuthHeaders(),
          credentials: "include",
        });

        if (!response.ok) {
          console.error(
            `[FETCH ERROR] Atlas sprite sheet API returned ${response.status} for item: ${item_key}`,
          );
          return 0;
        }

        const data = await response.text();

        const buffer_size = lengthBytesUTF8(data) + 1;
        const buffer = _malloc(buffer_size);

        if (!buffer) {
          console.error(
            "[FETCH ERROR] Memory allocation failed for atlas sprite sheet response",
          );
          return 0;
        }

        stringToUTF8(data, buffer, buffer_size);
        return buffer;
      } catch (error) {
        console.error(
          `[FETCH ERROR] Failed to fetch atlas sprite sheet for ${item_key}:`,
          error.message,
        );
        return 0;
      }
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
  js_fetch_object_layer__deps: ["$FetchState", "$buildAuthHeaders"],
  js_fetch_object_layer: function (item_id_ptr) {
    const item_id = UTF8ToString(item_id_ptr);

    const filterModel = JSON.stringify({
      "data.item.id": {
        filterType: "text",
        type: "equals",
        filter: item_id,
      },
    });

    const url = `${FetchState.api_base_url}/api/object-layer/?filterModel=${encodeURIComponent(filterModel)}&limit=1`;

    return Asyncify.handleAsync(async () => {
      try {
        const response = await fetch(url, {
          method: "GET",
          headers: buildAuthHeaders(),
          credentials: "include",
        });

        if (!response.ok) {
          console.error(
            `[FETCH ERROR] Object layer API returned ${response.status} for item: ${item_id}`,
          );
          return 0;
        }

        const data = await response.text();

        const buffer_size = lengthBytesUTF8(data) + 1;
        const buffer = _malloc(buffer_size);

        if (!buffer) {
          console.error(
            "[FETCH ERROR] Memory allocation failed for object layer response",
          );
          return 0;
        }

        stringToUTF8(data, buffer, buffer_size);
        return buffer;
      } catch (error) {
        console.error(
          `[FETCH ERROR] Failed to fetch object layer for ${item_id}:`,
          error.message,
        );
        return 0;
      }
    });
  },

  /**
   * @brief Start an asynchronous binary fetch (non-blocking)
   *
   * Used to fetch atlas PNG blobs from the file API:
   *   GET {api_base_url}/api/file/blob/{fileId}
   *
   * The URL is constructed on the C side and passed in full.
   *
   * @param url_ptr Pointer to URL string
   * @param request_id Unique ID for this request
   */
  js_start_fetch_binary__deps: ["$FetchState", "$buildAuthHeaders"],
  js_start_fetch_binary: function (url_ptr, request_id) {
    const url = UTF8ToString(url_ptr);

    // Prevent duplicate requests
    if (FetchState.active[request_id] || FetchState.completed[request_id]) {
      return;
    }

    FetchState.active[request_id] = true;

    const headers = buildAuthHeaders();
    // Remove Accept: application/json for binary requests
    delete headers["Accept"];

    fetch(url, {
      method: "GET",
      headers: headers,
      credentials: "include",
    })
      .then((response) => {
        if (!response.ok) throw new Error(`HTTP ${response.status}`);
        return response.arrayBuffer();
      })
      .then((buffer) => {
        const byteArray = new Uint8Array(buffer);
        FetchState.completed[request_id] = {
          data: byteArray,
          status: 1, // Success
        };
        delete FetchState.active[request_id];
      })
      .catch((err) => {
        console.error(
          `[FETCH ERROR] Async binary fetch failed for ${url}:`,
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
    const result = FetchState.completed[request_id];

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
    const byteArray = result.data;
    const size = byteArray.length;
    const buffer = _malloc(size);

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
