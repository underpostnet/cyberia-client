#ifndef CONFIG_H
#define CONFIG_H

// ========== WebSocket Configuration ==========
extern const char* WS_URL;

// ========== API Configuration ==========
extern const char* API_BASE_URL;

// ========== Assets Configuration ==========
extern const char* ASSETS_BASE_URL;

// ========== Game Configuration ==========
extern const char* GHOST_ITEM_ID;

// ========== Network Configuration ==========
extern const long HTTP_TIMEOUT_SECONDS;

// ========== Cache Configuration ==========
extern const int MAX_TEXTURE_CACHE_SIZE;
extern const int MAX_LAYER_CACHE_SIZE;
extern const int TEXTURE_QUEUE_SIZE;

// ========== Animation Configuration ==========
extern const int DEFAULT_FRAME_DURATION_MS;

#endif // CONFIG_H