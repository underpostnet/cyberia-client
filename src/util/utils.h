#ifndef CYBERIA_UTIL_UTILS_H
#define CYBERIA_UTIL_UTILS_H

#include <stddef.h>
#include <cJSON.h>

/* Small helpers with no home of their own. */

/* Copies `src` into a `cap`-byte buffer and always terminates it.
 * A NULL `src` gives an empty string. */
void copy_str(char* dst, size_t cap, const char* src);

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

#endif /* CYBERIA_UTIL_UTILS_H */
