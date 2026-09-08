#include "network/engine_client.c"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static double clock_ms;
static int network_count;
static int callbacks;
static int failures;
static int freed_textures;
static int decodes;
static int scales[2048];
static int status[2048];
static int image_width = 32;
static int image_height = 32;
static size_t copy_max;
static size_t body_size = 131072;
static void* held;
static char order[2048][80];

const char* config_data_server_url(void) { return "https://content.test"; }
double asset_bridge_now(void) { return clock_ms; }
unsigned asset_bridge_diagnostics(void) { return 0; }
void asset_bridge_trace(const char* event, const char* id, double bytes, double duration, int priority, const char* consumer) {}
int asset_bridge_fetch(const char* url, unsigned timeout, size_t limit) {
    int handle = ++network_count;
    snprintf(order[handle], sizeof(order[handle]), "%s", url);
    return handle;
}
int asset_bridge_status(int handle) { return status[handle]; }
size_t asset_bridge_size(int handle) { return body_size; }
void asset_bridge_copy(int handle, void* destination, size_t offset, size_t size) {
    if (copy_max < size) copy_max = size;
    memset(destination, 42, size);
    clock_ms += 0.4;
}
void asset_bridge_release(int handle) {}
void asset_bridge_decode_image(int handle, int scale) { scales[handle] = scale; status[handle] = 2; decodes++; }
int asset_bridge_image_width(int handle) { return image_width; }
int asset_bridge_image_height(int handle) { return image_height; }
void asset_bridge_upload(int handle, unsigned texture, int row, int count) { clock_ms += 0.4; }
unsigned int rlLoadTexture(const void* data, int width, int height, int format, int mipmaps) { return 77; }
void UnloadTexture(Texture2D texture) { assert(77 == texture.id); freed_textures++; }

static void receive(const FetchResponse* response) {
    callbacks++;
    if (!response->success) { failures++; return; }
    assert(42 == ((unsigned char*)response->data)[0]);
    if (NULL == held) held = response->data;
    else { assert(held == response->data); fetch_data_release(response->data); }
}
static void release(const FetchResponse* response) {
    callbacks++;
    if (!response->success) failures++;
    fetch_data_release(response->data);
}
static void frame(void) {
    clock_ms += 16.7;
    fetch_frame_begin(16.7, false);
    fetch_process_frame();
}
static void settle(void) {
    for (int i = 0; 5000 > i && 0 < fetch_pending_count(); i++) {
        for (int j = 1; network_count >= j; j++) {
            if (0 == status[j]) status[j] = 1;
            else if (2 == status[j]) status[j] = 3;
        }
        frame();
    }
    assert(0 == fetch_pending_count());
}

int main(void) {
    fetch_init();
    fetch_set_max_concurrent(1);
    fetch_request_start_at("background", "/p2", release, FETCH_P2);
    fetch_request_start_at("player", "/p0", release, FETCH_P0);
    frame();
    assert(1 == network_count && NULL != strstr(order[1], "/p0"));
    assert(1 == fetch_in_flight_count());
    status[1] = 1;
    frame();
    assert(2 == network_count && 0 == callbacks);
    settle();
    assert(2 == callbacks);

    int before = network_count;
    callbacks = 0;
    fetch_request_start("audio-token", "/shared", receive);
    fetch_request_start("map-token", "/shared", receive);
    fetch_request_start("audio-token", "/shared", receive);
    frame();
    assert(before + 1 == network_count);
    assert(0 == callbacks);
    status[network_count] = 1;
    frame();
    assert(0 == callbacks && 16384 >= copy_max);
    settle();
    assert(2 == callbacks && NULL != held);
    fetch_request_start("again", "/shared", receive);
    settle();
    assert(3 == callbacks && before + 1 == network_count);
    fetch_data_release(held);
    held = NULL;

    before = network_count;
    fetch_request_start("same-id", "/metadata", release);
    fetch_request_start("same-id", "/blob", release);
    settle();
    assert(before + 2 == network_count);

    before = network_count;
    assert(0 == fetch_texture("/image", 20, FETCH_P2, false).id);
    frame();
    status[network_count] = 1;
    frame();
    assert(ASSET_PROCESSING == fetch_state("/image"));
    assert(20 == scales[network_count]);
    fetch_texture("/image", 20, FETCH_P0, true);
    assert(1 == decodes && before + 1 == network_count);
    fetch_texture("/player-image", 20, FETCH_P0, true);
    frame();
    assert(before + 2 == network_count);
    status[network_count] = 1;
    frame();
    assert(2 == decodes);
    settle();
    assert(77 == fetch_texture("/image", 20, FETCH_P0, true).id);
    assert(77 == fetch_texture("/player-image", 20, FETCH_P0, true).id);
    assert(before + 2 == network_count && 2 == decodes && 0 == freed_textures);

    fetch_request_start("failure", "/failure", release);
    frame();
    status[network_count] = -1;
    frame();
    assert(1 == failures && ASSET_FAILED == fetch_state("/failure"));
    before = network_count;
    fetch_request_start("failure", "/failure", release);
    settle();
    assert(before == network_count && 2 == failures);
    clock_ms += FETCH_RETRY_MS;
    fetch_request_start("failure", "/failure", release);
    settle();
    assert(before + 1 == network_count && ASSET_READY == fetch_state("/failure"));

    fetch_shutdown();
    assert(2 == freed_textures);
    before = failures;
    for (int i = 0; FETCH_QUEUE_CAP > i; i++) {
        char url[40];
        snprintf(url, sizeof(url), "/burst/%d", i);
        fetch_request_start_at(url, url, release, FETCH_P2);
    }
    fetch_request_start_at("overflow", "/overflow", release, FETCH_P2);
    assert(before + 1 == failures);
    fetch_request_start_at("player", "/urgent", release, FETCH_P0);
    assert(before + 2 == failures);
    assert(ASSET_QUEUED == fetch_state("/urgent"));
    before = network_count;
    frame();
    assert(NULL != strstr(order[before + 1], "/urgent"));
    fetch_set_max_concurrent(10000);
    assert(64 == fetch_max_concurrent());
    frame();
    assert(64 >= fetch_in_flight_count());
    settle();
    fetch_shutdown();

    body_size = 1024;
    image_width = image_height = 4096;
    fetch_texture("/large", 1, FETCH_P1, true);
    settle();
    frame();
    fetch_texture("/current", 1, FETCH_P0, true);
    settle();
    frame();
    assert(ASSET_UNREQUESTED == fetch_state("/large"));
    assert(ASSET_READY == fetch_state("/current"));
    fetch_shutdown();
    assert(0 == fetch_pending_count() && 0 == fetch_in_flight_count());
    puts("asset request tests passed");
}
