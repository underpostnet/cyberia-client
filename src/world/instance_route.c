#include "instance_route.h"

#include <stdio.h>

void instance_route_sanitize_code(char* code) {
    size_t write = 0;
    for (size_t read = 0; '\0' != code[read]; ++read) {
        const char c = code[read];
        if (('a' <= c && 'z' >= c) || ('A' <= c && 'Z' >= c) || ('0' <= c && '9' >= c) || '-' == c || '_' == c)
            code[write++] = c;
    }
    code[write] = '\0';
}

void instance_route_ws_url(const char* origin, const char* instance_code, char* out, size_t out_size) {
    if (!instance_code || '\0' == instance_code[0])
        snprintf(out, out_size, "%s/ws", origin);
    else
        snprintf(out, out_size, "%s/%s/ws", origin, instance_code);
}
