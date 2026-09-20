#ifndef CYBERIA_UTIL_UTILS_H
#define CYBERIA_UTIL_UTILS_H

#include <stddef.h>
#include <cJSON.h>

/* Small helpers with no home of their own. */

/* Copies `src` into a `cap`-byte buffer and always terminates it.
 * A NULL `src` gives an empty string. */
void copy_str(char* dst, size_t cap, const char* src);

/* Percent-encodes `src` into a `cap`-byte buffer and always terminates it.
 * RFC 3986 unreserved bytes pass through. Output stops before an escape that
 * does not fit. */
void url_encode(char* dst, size_t cap, const char* src);

/* Takes a slot from a fixed array: the first free one, else the stalest.
 * The slot comes back zeroed; the caller marks it in use.
 *
 * `rank` scores one element. The highest rank wins, so a free element must
 * return INFINITY and a live one must return its staleness. */
void* pool_take(void* base, size_t elem_size, int count,
                float (*rank)(const void* elem));

/* Returns the `data` object of a {"status":"success","data":{...}} envelope,
 * else NULL. */
const cJSON* envelope_success_doc(const cJSON* root);

/* Reads one field of a JSON object. A missing field, or one of the wrong
 * type, gives NULL / `fallback`. */
static inline const char* json_str(const cJSON* obj, const char* key) {
    const cJSON* v = cJSON_GetObjectItemCaseSensitive(obj, key);
    return cJSON_IsString(v) ? v->valuestring : NULL;
}

static inline int json_int(const cJSON* obj, const char* key, int fallback) {
    const cJSON* v = cJSON_GetObjectItemCaseSensitive(obj, key);
    return cJSON_IsNumber(v) ? v->valueint : fallback;
}

#endif /* CYBERIA_UTIL_UTILS_H */
