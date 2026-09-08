#include "instance_route.h"

#include <stdio.h>
#include <string.h>

void instance_route_sanitize_code(char* code) {
    size_t write = 0;
    for (size_t read = 0; '\0' != code[read]; ++read) {
        const char c = code[read];
        if (('a' <= c && 'z' >= c) || ('A' <= c && 'Z' >= c) || ('0' <= c && '9' >= c) || '-' == c || '_' == c)
            code[write++] = c;
    }
    code[write] = '\0';
}

void instance_route_ws_url(const char* runtime_origin, const char* fallback_endpoint,
                           const char* instance_code, char* out, size_t out_size) {
    char origin[512] = {0};

    if (runtime_origin && '\0' != runtime_origin[0]) {
        snprintf(origin, sizeof(origin), "%s", runtime_origin);
    } else if (fallback_endpoint) {
        snprintf(origin, sizeof(origin), "%s", fallback_endpoint);
        const size_t len = strlen(origin);
        if (3 <= len && 0 == strcmp(origin + len - 3, "/ws")) origin[len - 3] = '\0';
    }

    if (!instance_code || '\0' == instance_code[0])
        snprintf(out, out_size, "%s/ws", origin);
    else
        snprintf(out, out_size, "%s/%s/ws", origin, instance_code);
}
