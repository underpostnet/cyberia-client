#include "runtime_config.h"
#include "config.h"
#include "util/log.h"

#include <assert.h>
#include <emscripten/emscripten.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RC_ORIGIN_MAX 256
#define RC_CODE_MAX   128
#define RC_URL_MAX    512

static char s_instance_code[RC_CODE_MAX];
static char s_ws_url[RC_URL_MAX];
static char s_api_base_url[RC_URL_MAX];
static bool s_initialized = false;

/* Copies a JS-side string allocated with allocateUTF8 into out, freeing it. */
static void adopt_js_string(char* js_string, char* out, size_t out_size) {
    assert(js_string);
    snprintf(out, out_size, "%s", js_string);
    free(js_string);
}

void runtime_config_init(void) {
    if (s_initialized) return;
    s_initialized = true;

    /* The instance code is the first URL path segment; an empty segment (root)
     * means the default instance, served bare by the origin. The default is not
     * folded in here: at root the ws URL stays prefix-free so the server's `/`
     * route serves it, keeping the client and the proxy on one routing rule. */
    adopt_js_string((char*)EM_ASM_PTR({
                        var segment = (self.location && self.location.pathname ? self.location.pathname : "/")
                                          .split("/")
                                          .filter(function(part) { return part.length > 0; })[0] ||
                                      "";
                        return allocateUTF8(segment);
                    }),
                    s_instance_code, sizeof(s_instance_code));

    /* The code arrives from the URL and is interpolated into the websocket URL,
     * so drop everything outside the legal code alphabet. */
    size_t write = 0;
    for (size_t read = 0; '\0' != s_instance_code[read]; ++read) {
        const char c = s_instance_code[read];
        if (('a' <= c && 'z' >= c) || ('A' <= c && 'Z' >= c) || ('0' <= c && '9' >= c) || '-' == c || '_' == c)
            s_instance_code[write++] = c;
    }
    s_instance_code[write] = '\0';

    char ws_origin[RC_ORIGIN_MAX] = {0};
    adopt_js_string((char*)EM_ASM_PTR({ return allocateUTF8(self.CYBERIA_WS_ORIGIN || ""); }), ws_origin,
                    sizeof(ws_origin));

    /* Compile-time WS_URL is a full endpoint ("wss://host/ws"); the runtime form
     * is an origin the instance segment is appended to, so strip the suffix. */
    if ('\0' == ws_origin[0]) {
        snprintf(ws_origin, sizeof(ws_origin), "%s", WS_URL);
        const size_t len = strlen(ws_origin);
        if (3 <= len && 0 == strcmp(ws_origin + len - 3, "/ws")) ws_origin[len - 3] = '\0';
    }

    if ('\0' == s_instance_code[0])
        snprintf(s_ws_url, sizeof(s_ws_url), "%s/ws", ws_origin);
    else
        snprintf(s_ws_url, sizeof(s_ws_url), "%s/%s/ws", ws_origin, s_instance_code);

    adopt_js_string((char*)EM_ASM_PTR({ return allocateUTF8(self.CYBERIA_ENGINE_API_ORIGIN || ""); }), s_api_base_url,
                    sizeof(s_api_base_url));
    if ('\0' == s_api_base_url[0]) snprintf(s_api_base_url, sizeof(s_api_base_url), "%s", API_BASE_URL);

    LOG_INFO("runtime config instance=%s ws=%s api=%s", s_instance_code, s_ws_url, s_api_base_url);
}

const char* runtime_config_instance_code(void) {
    return s_instance_code;
}

const char* runtime_config_ws_url(void) {
    return s_ws_url;
}

const char* runtime_config_api_base_url(void) {
    return s_api_base_url;
}
