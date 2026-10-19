#ifndef CYBERIA_TEST_WEBSOCKET_H
#define CYBERIA_TEST_WEBSOCKET_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

typedef bool EM_BOOL;
typedef int EMSCRIPTEN_WEBSOCKET_T;
typedef int EMSCRIPTEN_RESULT;
#define EM_TRUE true
#define EM_FALSE false
#define EMSCRIPTEN_RESULT_SUCCESS 0

typedef struct { EMSCRIPTEN_WEBSOCKET_T socket; } EmscriptenWebSocketOpenEvent;
typedef struct { EMSCRIPTEN_WEBSOCKET_T socket; } EmscriptenWebSocketErrorEvent;
typedef struct {
    EMSCRIPTEN_WEBSOCKET_T socket;
    uint8_t* data;
    uint32_t numBytes;
    bool isText;
} EmscriptenWebSocketMessageEvent;
typedef struct {
    EMSCRIPTEN_WEBSOCKET_T socket;
    bool wasClean;
    unsigned short code;
    char reason[512];
} EmscriptenWebSocketCloseEvent;
typedef struct {
    const char* url;
    const char* protocols;
    bool createOnMainThread;
} EmscriptenWebSocketCreateAttributes;

typedef EM_BOOL (*em_websocket_open_callback_func)(int, const EmscriptenWebSocketOpenEvent*, void*);
typedef EM_BOOL (*em_websocket_message_callback_func)(int, const EmscriptenWebSocketMessageEvent*, void*);
typedef EM_BOOL (*em_websocket_error_callback_func)(int, const EmscriptenWebSocketErrorEvent*, void*);
typedef EM_BOOL (*em_websocket_close_callback_func)(int, const EmscriptenWebSocketCloseEvent*, void*);

#define emscripten_websocket_init_create_attributes(attrs) memset(attrs, 0, sizeof *(attrs))
bool emscripten_websocket_is_supported(void);
EMSCRIPTEN_WEBSOCKET_T emscripten_websocket_new(EmscriptenWebSocketCreateAttributes* attrs);
EMSCRIPTEN_RESULT emscripten_websocket_get_ready_state(EMSCRIPTEN_WEBSOCKET_T socket, unsigned short* state);
EMSCRIPTEN_RESULT emscripten_websocket_send_binary(EMSCRIPTEN_WEBSOCKET_T socket, void* data, uint32_t length);
EMSCRIPTEN_RESULT emscripten_websocket_close(EMSCRIPTEN_WEBSOCKET_T socket, unsigned short code, const char* reason);
EMSCRIPTEN_RESULT emscripten_websocket_delete(EMSCRIPTEN_WEBSOCKET_T socket);
EMSCRIPTEN_RESULT emscripten_websocket_set_onopen_callback(EMSCRIPTEN_WEBSOCKET_T socket, void* ctx,
                                                          em_websocket_open_callback_func callback);
EMSCRIPTEN_RESULT emscripten_websocket_set_onmessage_callback(EMSCRIPTEN_WEBSOCKET_T socket, void* ctx,
                                                             em_websocket_message_callback_func callback);
EMSCRIPTEN_RESULT emscripten_websocket_set_onerror_callback(EMSCRIPTEN_WEBSOCKET_T socket, void* ctx,
                                                           em_websocket_error_callback_func callback);
EMSCRIPTEN_RESULT emscripten_websocket_set_onclose_callback(EMSCRIPTEN_WEBSOCKET_T socket, void* ctx,
                                                           em_websocket_close_callback_func callback);

#endif
