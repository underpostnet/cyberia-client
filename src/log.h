#ifndef CYBERIA_UTIL_LOG_H
#define CYBERIA_UTIL_LOG_H

/* Structured logging macros.
 *
 *   LOG_LEVEL_DEBUG  4   chatty heartbeats, per-frame state
 *   LOG_LEVEL_INFO   3   one-shot lifecycle events
 *   LOG_LEVEL_WARN   2   recoverable degradations
 *   LOG_LEVEL_ERROR  1   non-recoverable failures
 *   LOG_LEVEL_NONE   0
 *
 * Compile-time threshold via -DCYBERIA_LOG_LEVEL=N. Defaults to INFO for
 * release builds and DEBUG when NDEBUG is undefined.
 */

#include <stdio.h>

#define LOG_LEVEL_NONE   0
#define LOG_LEVEL_ERROR  1
#define LOG_LEVEL_WARN   2
#define LOG_LEVEL_INFO   3
#define LOG_LEVEL_DEBUG  4

#ifndef CYBERIA_LOG_LEVEL
#  ifdef NDEBUG
#    define CYBERIA_LOG_LEVEL LOG_LEVEL_INFO
#  else
#    define CYBERIA_LOG_LEVEL LOG_LEVEL_DEBUG
#  endif
#endif

#if CYBERIA_LOG_LEVEL >= LOG_LEVEL_DEBUG
#  define LOG_DEBUG(fmt, ...) fprintf(stdout, "[DBG] " fmt "\n", ##__VA_ARGS__)
#else
#  define LOG_DEBUG(fmt, ...) ((void)0)
#endif

#if CYBERIA_LOG_LEVEL >= LOG_LEVEL_INFO
#  define LOG_INFO(fmt, ...)  fprintf(stdout, "[INF] " fmt "\n", ##__VA_ARGS__)
#else
#  define LOG_INFO(fmt, ...)  ((void)0)
#endif

#if CYBERIA_LOG_LEVEL >= LOG_LEVEL_WARN
#  define LOG_WARN(fmt, ...)  fprintf(stderr, "[WRN] " fmt "\n", ##__VA_ARGS__)
#else
#  define LOG_WARN(fmt, ...)  ((void)0)
#endif

#if CYBERIA_LOG_LEVEL >= LOG_LEVEL_ERROR
#  define LOG_ERROR(fmt, ...) fprintf(stderr, "[ERR] " fmt "\n", ##__VA_ARGS__)
#else
#  define LOG_ERROR(fmt, ...) ((void)0)
#endif

#endif /* CYBERIA_UTIL_LOG_H */
