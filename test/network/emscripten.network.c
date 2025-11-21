#include "../../src_c/network.h"
#include "../../src_c/config.h"
#include <stdio.h>
#include <string.h>
#include <emscripten.h>

// Global WebSocket client instance
static WebSocketClient ws_client;
static int message_count = 0;
static WebSocketHandlers handlers; // Make handlers static so it persists

// Test callback: on_open
void test_on_open(void* user_data) {
    printf("\n========================================\n");
    printf("EVENT: on_open\n");
    printf("========================================\n");
    printf("[LOG] WebSocket connection successfully established\n");
    printf("[LOG] Connection URL: %s\n", WS_URL);
    printf("[LOG] Ready state: OPEN\n");
    printf("[LOG] Timestamp: %d\n", (int)emscripten_get_now());

    // Mark client as connected
    ws_client.connected = 1;
    printf("[LOG] Client marked as connected: %s\n", ws_client.connected ? "YES" : "NO");

    // Send a test message
    const char* test_msg = "{\"type\":\"hello\",\"message\":\"Hello from Cyberia WebSocket Test Client!\"}";
    printf("[LOG] Sending initial test message: %s\n", test_msg);

    int result = ws_send(&ws_client, test_msg, strlen(test_msg));
    if (result == 0) {
        printf("[LOG] Message sent successfully\n");
    } else {
        printf("[ERROR] Failed to send message, error code: %d\n", result);
    }
    printf("========================================\n\n");
}

// Test callback: on_message
void test_on_message(const char* data, int length, void* user_data) {
    message_count++;

    printf("\n========================================\n");
    printf("EVENT: on_message (Message #%d)\n", message_count);
    printf("========================================\n");
    printf("[LOG] Message received from server\n");
    printf("[LOG] Message length: %d bytes\n", length);
    printf("[LOG] Timestamp: %d\n", (int)emscripten_get_now());
    printf("[LOG] Message content:\n");
    printf("----------------------------------------\n");

    // Print message content (assuming it's text)
    for (int i = 0; i < length; i++) {
        printf("%c", data[i]);
    }
    printf("\n");
    printf("----------------------------------------\n");

    // Print hex dump for debugging
    printf("[LOG] Hex dump (first 64 bytes):\n");
    int hex_limit = length < 64 ? length : 64;
    for (int i = 0; i < hex_limit; i++) {
        printf("%02X ", (unsigned char)data[i]);
        if ((i + 1) % 16 == 0) printf("\n");
    }
    if (hex_limit % 16 != 0) printf("\n");

    printf("[LOG] Total messages received: %d\n", message_count);

    // Echo back a response after 3 seconds (for testing)
    if (message_count == 1) {
        printf("[LOG] Will send echo response...\n");
        const char* response = "{\"type\":\"echo\",\"original_length\":%d}";
        char buffer[256];
        snprintf(buffer, sizeof(buffer), response, length);
        ws_send(&ws_client, buffer, strlen(buffer));
        printf("[LOG] Echo response sent\n");
    }

    printf("========================================\n\n");
}

// Test callback: on_error
void test_on_error(void* user_data) {
    printf("\n========================================\n");
    printf("EVENT: on_error\n");
    printf("========================================\n");
    printf("[ERROR] WebSocket error occurred!\n");
    printf("[ERROR] Connection URL: %s\n", WS_URL);
    printf("[ERROR] Timestamp: %d\n", (int)emscripten_get_now());
    printf("[LOG] Marking client as disconnected\n");

    ws_client.connected = 0;

    printf("[LOG] Client connected status: %s\n", ws_client.connected ? "YES" : "NO");
    printf("[LOG] Socket handle: %d\n", ws_client.socket);
    printf("[ERROR] Check server availability and network connection\n");
    printf("========================================\n\n");
}

// Test callback: on_close
void test_on_close(int code, const char* reason, void* user_data) {
    printf("\n========================================\n");
    printf("EVENT: on_close\n");
    printf("========================================\n");
    printf("[LOG] WebSocket connection closed\n");
    printf("[LOG] Close code: %d\n", code);
    printf("[LOG] Close reason: %s\n", reason ? reason : "(no reason provided)");
    printf("[LOG] Timestamp: %d\n", (int)emscripten_get_now());

    // Interpret close codes
    printf("[LOG] Close code meaning: ");
    switch (code) {
        case 1000:
            printf("Normal Closure\n");
            break;
        case 1001:
            printf("Going Away\n");
            break;
        case 1002:
            printf("Protocol Error\n");
            break;
        case 1003:
            printf("Unsupported Data\n");
            break;
        case 1006:
            printf("Abnormal Closure (no close frame)\n");
            break;
        case 1007:
            printf("Invalid Frame Payload Data\n");
            break;
        case 1008:
            printf("Policy Violation\n");
            break;
        case 1009:
            printf("Message Too Big\n");
            break;
        case 1011:
            printf("Internal Server Error\n");
            break;
        default:
            printf("Unknown (%d)\n", code);
            break;
    }

    ws_client.connected = 0;
    printf("[LOG] Client connected status: %s\n", ws_client.connected ? "YES" : "NO");
    printf("[LOG] Total messages received during session: %d\n", message_count);
    printf("========================================\n\n");
}

// Main test function
int main() {
    printf("\n");
    printf("************************************************\n");
    printf("*                                              *\n");
    printf("*   Cyberia Client - WebSocket Test Suite     *\n");
    printf("*                                              *\n");
    printf("************************************************\n");
    printf("\n");

    printf("[INIT] Starting WebSocket test...\n");
    printf("[INIT] Test start time: %d\n", (int)emscripten_get_now());
    printf("[INIT] Target URL: %s\n", WS_URL);
    printf("\n");

    // Check WebSocket support
    if (!emscripten_websocket_is_supported()) {
        printf("[FATAL] WebSocket is not supported in this environment!\n");
        printf("[FATAL] Test cannot continue.\n");
        return 1;
    }
    printf("[INIT] WebSocket support: CONFIRMED\n");

    // Setup event handlers (using static global handlers)
    handlers.on_open = test_on_open;
    handlers.on_message = test_on_message;
    handlers.on_error = test_on_error;
    handlers.on_close = test_on_close;
    handlers.user_data = NULL; // Can pass custom test data here

    printf("[INIT] Event handlers configured:\n");
    printf("       - on_open:    %p\n", (void*)handlers.on_open);
    printf("       - on_message: %p\n", (void*)handlers.on_message);
    printf("       - on_error:   %p\n", (void*)handlers.on_error);
    printf("       - on_close:   %p\n", (void*)handlers.on_close);
    printf("\n");

    // Initialize WebSocket client using WS_URL from config
    printf("[INIT] Initializing WebSocket client...\n");
    int result = ws_init(&ws_client, WS_URL, &handlers);

    if (result != 0) {
        printf("[FATAL] Failed to initialize WebSocket client\n");
        printf("[FATAL] Error code: %d\n", result);
        printf("[FATAL] Test FAILED\n");
        return 1;
    }

    printf("[INIT] WebSocket client initialized successfully\n");
    printf("[INIT] Socket handle: %d\n", ws_client.socket);
    printf("[INIT] Initial connection status: %s\n", ws_client.connected ? "CONNECTED" : "CONNECTING");
    printf("\n");
    printf("************************************************\n");
    printf("*   Waiting for WebSocket events...           *\n");
    printf("************************************************\n");
    printf("\n");

    // The event loop continues in the browser environment
    // Emscripten handles this automatically via the event system

    printf("[INFO] Event loop running. Events will be logged as they occur.\n");
    printf("[INFO] Keep this page open to monitor WebSocket activity.\n");
    printf("\n");

    return 0;
}
