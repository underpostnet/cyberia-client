#include "config.h"

// WebSocket configuration
const char* WS_URL = "wss://server.cyberiaonline.com/ws";
// const char* WS_URL = "ws://localhost:8080/ws";

// API configuration
const char* API_BASE_URL = "https://server.cyberiaonline.com/api/v1";
// const char* API_BASE_URL = "http://localhost:8080/api/v1";

// Assets configuration
const char* ASSETS_BASE_URL = "https://server.cyberiaonline.com/assets";
// const char* ASSETS_BASE_URL = "http://localhost:8080/assets";

// Ghost item ID - shown when player/bot is dead
const char* GHOST_ITEM_ID = "ghost";

// HTTP timeout in seconds
const long HTTP_TIMEOUT_SECONDS = 10L;

// Cache settings
const int MAX_TEXTURE_CACHE_SIZE = 512;
const int MAX_LAYER_CACHE_SIZE = 256;
const int TEXTURE_QUEUE_SIZE = 1024;

// Animation defaults
const int DEFAULT_FRAME_DURATION_MS = 100;