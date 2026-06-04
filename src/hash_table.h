#ifndef HASH_TABLE_H
#define HASH_TABLE_H

#include <stdbool.h>
#include <stddef.h>

/*
 * Generic string-keyed hash table — open addressing with linear probing.
 *
 * - Keys are duplicated on insert (strdup) and freed on remove/destroy.
 * - Values are opaque void*; ownership is controlled by `free_fn`.
 *     - If non-NULL, hash_table_destroy / _put-replace / _remove call it on
 *       the value being dropped.
 *     - If NULL, caller retains ownership.
 * - Capacity grows automatically; entries are rehashed on resize, so
 *   pointers to internal slots are NOT stable across put/remove.
 * - Not thread-safe.
 */

typedef unsigned long hash_t;
typedef void (*HashFreeFn)(void* value);

/* djb2 string hash */
static inline hash_t hash_string(const char* str) {
    hash_t hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

typedef enum {
    SLOT_EMPTY = 0,
    SLOT_OCCUPIED,
    SLOT_TOMBSTONE
} SlotState;

typedef struct {
    char*     key;
    void*     value;
    SlotState state;
} HashSlot;

typedef struct {
    HashSlot*   slots;
    size_t      capacity;
    size_t      count;        /* occupied slots (excludes tombstones) */
    size_t      tombstones;
    HashFreeFn  free_fn;
    const char* debug_name;   /* identifies table in error logs; not owned */
} HashTable;

/* Lifecycle */
void hash_table_init(HashTable* t, size_t initial_capacity, HashFreeFn free_fn, const char* debug_name);
void hash_table_destroy(HashTable* t);

/* Core ops */
void* hash_table_get(const HashTable* t, const char* key);
void  hash_table_put(HashTable* t, const char* key, void* value);
bool  hash_table_remove(HashTable* t, const char* key);
bool  hash_table_contains(const HashTable* t, const char* key);

/* Iterate every occupied slot. Do not insert/remove during iteration. */
// typedef void (*HashIterFn)(const char* key, void* value, void* user_data);
// void hash_table_foreach(const HashTable* t, HashIterFn fn, void* user_data);

/* Linear scan with predicate; returns first matching value or NULL. */
typedef bool (*HashPredFn)(const char* key, void* value, void* user_data);
void* hash_table_find(const HashTable* t, HashPredFn pred, void* user_data);

/* Remove every occupied entry for which pred returns true; free_fn is applied
 * to each removed value. Safe to call mid-session. Returns count removed. */
size_t hash_table_remove_if(HashTable* t, HashPredFn pred, void* user_data);

#endif
