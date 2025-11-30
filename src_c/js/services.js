/**
 * @file services.js
 * @brief JavaScript service layer for HTTP requests from WebAssembly
 *
 * This file provides JavaScript-based fetch functions that can be called
 * from C/C++ code via Emscripten. Using JavaScript's native fetch API
 * is more reliable than Emscripten's fetch for handling CORS and async operations.
 */

mergeInto(LibraryManager.library, {
  // ==========================================================================
  // Internal State
  // ==========================================================================

  $FetchState: {
    // Configuration
    api_base_url: "https://server.cyberiaonline.com/api/v1",
    assets_base_url: "https://server.cyberiaonline.com/assets",

    // Async fetch tracking
    active: {},
    completed: {},
  },

  // ==========================================================================
  // Library Functions
  // ==========================================================================

  /**
   * @brief Set configuration from C code
   * This function is called from C to synchronize URLs with config.c
   */
  js_set_config__deps: ["$FetchState"],
  js_set_config: function (api_url_ptr, assets_url_ptr) {
    FetchState.api_base_url = UTF8ToString(api_url_ptr);
    FetchState.assets_base_url = UTF8ToString(assets_url_ptr);
  },

  /**
   * @brief Fetch object layer data from the API using JavaScript fetch
   *
   * This function performs an async HTTP GET request and returns the response
   * as a JSON string. It's called from C code and handles CORS properly.
   *
   * @param item_id_ptr Pointer to C string containing the item ID
   * @return Pointer to allocated string containing JSON response, or NULL on error
   */
  js_fetch_object_layer__deps: ["$FetchState"],
  js_fetch_object_layer: function (item_id_ptr) {
    const item_id = UTF8ToString(item_id_ptr);
    const url = `${FetchState.api_base_url}/object-layers?item_id=${item_id}&page=1&page_size=1`;

    // Use Asyncify to allow synchronous-like behavior from C
    return Asyncify.handleAsync(async () => {
      try {
        const response = await fetch(url, {
          method: "GET",
          headers: {
            Accept: "application/json",
          },
          credentials: "omit",
        });

        if (!response.ok) {
          console.error(
            `[FETCH ERROR] Object layer API returned ${response.status} for item: ${item_id}`,
          );
          return 0; // NULL pointer
        }

        const data = await response.text();

        // Allocate memory in WASM heap and copy the string
        const buffer_size = lengthBytesUTF8(data) + 1;
        const buffer = _malloc(buffer_size);

        if (!buffer) {
          console.error(
            "[FETCH ERROR] Memory allocation failed for object layer response",
          );
          return 0; // NULL pointer
        }

        stringToUTF8(data, buffer, buffer_size);
        return buffer;
      } catch (error) {
        console.error(
          `[FETCH ERROR] Failed to fetch object layer for ${item_id}:`,
          error.message,
        );
        return 0; // NULL pointer
      }
    });
  },

  /**
   * @brief Fetch binary data (e.g., images) from URL using JavaScript fetch
   *
   * @param url_ptr Pointer to C string containing the URL
   * @param size_ptr Pointer to size_t where the size will be stored
   * @return Pointer to allocated buffer containing binary data, or NULL on error
   */
  js_fetch_binary__deps: [],
  js_fetch_binary: function (url_ptr, size_ptr) {
    const url = UTF8ToString(url_ptr);

    return Asyncify.handleAsync(async () => {
      try {
        const response = await fetch(url, {
          method: "GET",
          credentials: "omit",
        });

        if (!response.ok) {
          console.error(
            `[FETCH ERROR] Binary fetch returned ${response.status}: ${url}`,
          );
          return 0; // NULL pointer
        }

        const arrayBuffer = await response.arrayBuffer();
        const byteArray = new Uint8Array(arrayBuffer);
        const size = byteArray.length;

        // Allocate memory in WASM heap
        const buffer = _malloc(size);
        if (!buffer) {
          console.error(
            "[FETCH ERROR] Memory allocation failed for binary data",
          );
          return 0; // NULL pointer
        }

        // Copy data to WASM heap
        HEAPU8.set(byteArray, buffer);

        // Write size to the pointer location
        HEAPU32[size_ptr >> 2] = size;

        return buffer;
      } catch (error) {
        console.error(
          `[FETCH ERROR] Binary fetch failed for ${url}:`,
          error.message,
        );
        return 0; // NULL pointer
      }
    });
  },

  /**
   * @brief Start an asynchronous binary fetch (non-blocking)
   *
   * @param url_ptr Pointer to URL string
   * @param request_id Unique ID for this request
   */
  js_start_fetch_binary__deps: ["$FetchState"],
  js_start_fetch_binary: function (url_ptr, request_id) {
    const url = UTF8ToString(url_ptr);

    // Prevent duplicate requests
    if (FetchState.active[request_id] || FetchState.completed[request_id]) {
      return;
    }

    FetchState.active[request_id] = true;

    fetch(url, { method: "GET", credentials: "omit" })
      .then((response) => {
        if (!response.ok) throw new Error(response.status);
        return response.arrayBuffer();
      })
      .then((buffer) => {
        const byteArray = new Uint8Array(buffer);
        // Store in JS memory until C claims it
        FetchState.completed[request_id] = {
          data: byteArray,
          status: 1, // Success
        };
        delete FetchState.active[request_id];
      })
      .catch((err) => {
        console.error(`[FETCH ERROR] Async fetch failed for ${url}:`, err);
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
      // Check if it's active or just unknown
      if (FetchState.active[request_id]) {
        HEAP32[size_ptr >> 2] = 0; // Pending
      } else {
        // Unknown ID, treat as error or not found
        HEAP32[size_ptr >> 2] = -1;
      }
      return 0;
    }

    if (result.status === -1) {
      HEAP32[size_ptr >> 2] = -1; // Error
      delete FetchState.completed[request_id];
      return 0;
    }

    // Success - allocate C memory and copy
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
