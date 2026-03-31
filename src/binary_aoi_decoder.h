/**
 * @file binary_aoi_decoder.h
 * @brief Decoder for the binary AOI protocol sent by the Go game server.
 *
 * This is the C/WASM client-side counterpart to aoi_binary.go.
 * When the server sends a WebSocket binary message, this decoder
 * parses it into the same game_state structures used by the JSON parser.
 *
 * Wire format documentation: see cyberia-server/src/aoi_binary.go
 */

#ifndef BINARY_AOI_DECODER_H
#define BINARY_AOI_DECODER_H

#include <stdint.h>
#include <stddef.h>

/* ── Message types ─────────────────────────────────────────────── */
#define BIN_MSG_AOI_UPDATE  0x01
#define BIN_MSG_INIT_DATA   0x02
#define BIN_MSG_FULL_AOI    0x03

/* ── Entity type bits (lower 3 bits of flags byte) ────────────── */
#define BIN_ENTITY_PLAYER      0
#define BIN_ENTITY_BOT         1
#define BIN_ENTITY_FLOOR       2
#define BIN_ENTITY_OBSTACLE    3
#define BIN_ENTITY_PORTAL      4
#define BIN_ENTITY_FOREGROUND  5

/* ── Flag bits ─────────────────────────────────────────────────── */
#define BIN_FLAG_REMOVED       0x08
#define BIN_FLAG_HAS_LIFE      0x10
#define BIN_FLAG_HAS_RESPAWN   0x20
#define BIN_FLAG_HAS_BEHAVIOR  0x40
#define BIN_FLAG_HAS_COLOR     0x80

/**
 * @brief Process a binary AOI message from the server.
 *
 * Parses the binary buffer and updates the global game state,
 * mirroring the behavior of message_parser_process() for JSON.
 *
 * @param data    Pointer to the raw binary message.
 * @param length  Length of the message in bytes.
 * @return 0 on success, -1 on error.
 */
int binary_aoi_process(const uint8_t* data, size_t length);

#endif /* BINARY_AOI_DECODER_H */
