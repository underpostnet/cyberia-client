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

  // Async quest-metadata fetch. The C side (quest_metadata_cache) is the only
  // authority on caching; we just hand back the raw envelope JSON so it can
  // parse title/description/steps/rewards. The C function tolerates an empty
  // body (marks the entry errored), so any failure path passes "".
  js_fetch_quest_metadata__deps: ['$FetchState'],
  js_fetch_quest_metadata: function (code_ptr) {
    var code = UTF8ToString(code_ptr);
    if (!code) return;

    var ingest = function (body) {
      var codePtr = allocateUTF8(code);
      var bodyPtr = allocateUTF8(body || '');
      Module._quest_metadata_cache_ingest_json(codePtr, bodyPtr);
      _free(codePtr);
      _free(bodyPtr);
    };

    var url = FetchState.api_base_url + '/api/cyberia-quest/code/' + encodeURIComponent(code);
    fetch(url, { headers: { Accept: 'application/json' } })
      .then(function (res) {
        return res.text();
      })
      .then(function (text) {
        ingest(text);
      })
      .catch(function (err) {
        console.warn('[API] quest metadata fetch failed for ' + code + ': ' + err);
        ingest('');
      });
  },
});
