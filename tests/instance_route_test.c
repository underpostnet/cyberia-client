/* URL-derived websocket routing: the multi-path client resolves its instance
 * from the first path segment, so every non-root path must dial its own
 * server mount instead of the default one. */

#include "instance_route.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static void expect_url(const char* runtime_origin, const char* code, const char* want) {
    char got[512] = {0};
    instance_route_ws_url(runtime_origin, "ws://localhost:8081/ws", code, got, sizeof(got));
    if (0 != strcmp(got, want)) {
        printf("ws url for origin=%s code=%s: got %s, want %s\n", runtime_origin, code, got, want);
        assert(false);
    }
}

static void expect_code(const char* raw, const char* want) {
    char got[128] = {0};
    snprintf(got, sizeof(got), "%s", raw);
    instance_route_sanitize_code(got);
    if (0 != strcmp(got, want)) {
        printf("sanitize %s: got %s, want %s\n", raw, got, want);
        assert(false);
    }
}

int main(void) {
    expect_url("ws://localhost:8081", "", "ws://localhost:8081/ws");
    expect_url("ws://localhost:8081", "TEST", "ws://localhost:8081/TEST/ws");
    expect_url("ws://localhost:8081", "FOREST", "ws://localhost:8081/FOREST/ws");
    expect_url("wss://server.cyberiaonline.com", "FOREST", "wss://server.cyberiaonline.com/FOREST/ws");

    /* No injected origin: the compile-time endpoint degrades to its origin. */
    expect_url("", "", "ws://localhost:8081/ws");
    expect_url("", "TEST", "ws://localhost:8081/TEST/ws");
    expect_url(NULL, "TEST", "ws://localhost:8081/TEST/ws");

    expect_code("TEST", "TEST");
    expect_code("my-world_2", "my-world_2");
    expect_code("../etc", "etc");
    expect_code("a b/c?d", "abcd");

    printf("instance_route_test OK\n");
    return 0;
}
