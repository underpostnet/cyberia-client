/**
 * @file services.js
 * @brief Engine API base URL holder for JS-side consumers.
 *
 * The C side fetches binaries via emscripten_fetch (see
 * network/engine_client.c), so this file no longer carries the C fetch
 * bridge. It now exists solely to hold the engine API base URL set from
 * C (js_init_engine_api → FetchState.api_base_url), consulted by
 * interact_overlay.js for DOM <img src> previews:
 *
 *   GET {api_base_url}/assets/{type}/{itemId}/08/0.png
 */

mergeInto(LibraryManager.library, {
  $FetchState: {
    // Engine API base URL (set from C config via js_init_engine_api)
    api_base_url: 'https://www.cyberiaonline.com',
  },

  js_init_engine_api__deps: ['$FetchState'],
  js_init_engine_api: function (api_base_url_ptr) {
    var api_base_url = UTF8ToString(api_base_url_ptr);

    if (api_base_url) {
      FetchState.api_base_url = api_base_url;
    }

    console.log('[API] Engine API base URL set to: ' + FetchState.api_base_url);
  },
});
