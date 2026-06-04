#include "hash_table.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "util/log.h"

/* Resize when (count + tombstones) exceeds this fraction of capacity. */
#define HASH_LOAD_NUM 7
#define HASH_LOAD_DEN 10

static size_t find_occupied(const HashTable* t, const char* key);
static size_t find_insert_slot(const HashTable* t, const char* key);
static void   resize(HashTable* t, size_t new_capacity);
static void   raw_insert(HashTable* t, char* key_owned, void* value);

void hash_table_init(HashTable* t, size_t initial_capacity, HashFreeFn free_fn, const char* debug_name) {
    assert(t);
    assert(initial_capacity > 0);
    assert(free_fn);
    assert(debug_name);
    t->slots      = calloc(initial_capacity, sizeof(HashSlot));
    t->capacity   = initial_capacity;
    t->count      = 0;
    t->tombstones = 0;
    t->free_fn    = free_fn;
    t->debug_name = debug_name;
}

void hash_table_destroy(HashTable* t) {
    if (NULL == t || NULL == t->slots) { return; }

    for (size_t i = 0; i < t->capacity; i++) {
        HashSlot* s = &t->slots[i];
        if (SLOT_OCCUPIED == s->state) {
            if (t->free_fn && s->value) { t->free_fn(s->value); }
            free(s->key);
        }
    }
    free(t->slots);
    t->slots      = NULL;
    t->capacity   = 0;
    t->count      = 0;
    t->tombstones = 0;
}

void* hash_table_get(const HashTable* t, const char* key) {
    assert(t);
    assert(key);
    size_t i = find_occupied(t, key);
    return (SIZE_MAX == i) ? NULL : t->slots[i].value;
}

bool hash_table_contains(const HashTable* t, const char* key) {
    assert(t);
    assert(key);
    return SIZE_MAX != find_occupied(t, key);
}

void hash_table_put(HashTable* t, const char* key, void* value) {
    assert(t);
    assert(key);
    assert(value);

    /* Resize before insert if at/above load threshold. */
    if ((t->count + t->tombstones + 1) * HASH_LOAD_DEN > t->capacity * HASH_LOAD_NUM) {
        size_t new_cap = t->capacity * 2;
        if (new_cap < t->capacity) { new_cap = t->capacity; } /* guard size_t overflow wrap */
        LOG_WARN("Hash Table '%s' resizing %zu -> %zu (count=%zu tombstones=%zu)",
                 t->debug_name, t->capacity, new_cap, t->count, t->tombstones);
        resize(t, new_cap);
    }

    size_t i = find_insert_slot(t, key);
    HashSlot* s = &t->slots[i];

    if (SLOT_OCCUPIED == s->state) {
        if (s->value != value) {
            t->free_fn(s->value);
        }
        s->value = value;
        return;
    }

    if (SLOT_TOMBSTONE == s->state) {
        assert(t->tombstones > 0);
        t->tombstones--;
    }

    s->key   = strdup(key);
    s->value = value;
    s->state = SLOT_OCCUPIED;
    t->count++;
}

bool hash_table_remove(HashTable* t, const char* key) {
    assert(t);
    assert(key);

    size_t i = find_occupied(t, key);
    if (SIZE_MAX == i) { return false; }

    HashSlot* s = &t->slots[i];
    if (t->free_fn && s->value) { t->free_fn(s->value); }
    free(s->key);
    s->key   = NULL;
    s->value = NULL;
    s->state = SLOT_TOMBSTONE;
    t->count--;
    t->tombstones++;
    return true;
}

void* hash_table_find(const HashTable* t, HashPredFn pred, void* user_data) {
    assert(t);
    assert(pred);
    for (size_t i = 0; i < t->capacity; i++) {
        if (SLOT_OCCUPIED == t->slots[i].state &&
            pred(t->slots[i].key, t->slots[i].value, user_data)) {
            return t->slots[i].value;
        }
    }
    return NULL;
}

size_t hash_table_remove_if(HashTable* t, HashPredFn pred, void* user_data) {
    assert(t);
    assert(pred);
    size_t removed = 0;
    for (size_t i = 0; i < t->capacity; i++) {
        HashSlot* s = &t->slots[i];
        if (SLOT_OCCUPIED != s->state) { continue; }
        if (!pred(s->key, s->value, user_data)) { continue; }
        if (t->free_fn && s->value) { t->free_fn(s->value); }
        free(s->key);
        s->key   = NULL;
        s->value = NULL;
        s->state = SLOT_TOMBSTONE;
        t->count--;
        t->tombstones++;
        removed++;
    }
    return removed;
}

/* ── Internals ──────────────────────────────────────────────────────── */

/* Probe until OCCUPIED-with-match or EMPTY. Skips tombstones.
 * Returns SIZE_MAX if not found. */
static size_t find_occupied(const HashTable* t, const char* key) {
    hash_t i = hash_string(key) % t->capacity;
    for (size_t probes = 0; probes < t->capacity; probes++) {
        HashSlot* s = &t->slots[i];
        if (SLOT_EMPTY == s->state) { return SIZE_MAX; }
        if (SLOT_OCCUPIED == s->state && 0 == strcmp(s->key, key)) {
            return (size_t)i;
        }
        i = (i + 1) % t->capacity;
    }
    return SIZE_MAX;
}

/* Returns slot index suitable for insert:
 *   - existing OCCUPIED slot with matching key (caller does replace), OR
 *   - first TOMBSTONE encountered on the probe path, OR
 *   - first EMPTY slot.
 * Capacity invariant (load factor < 1) guarantees an EMPTY exists. */
static size_t find_insert_slot(const HashTable* t, const char* key) {
    hash_t i = hash_string(key) % t->capacity;
    size_t first_tomb = SIZE_MAX;
    for (size_t probes = 0; probes < t->capacity; probes++) {
        HashSlot* s = &t->slots[i];
        if (SLOT_EMPTY == s->state) {
            return (SIZE_MAX != first_tomb) ? first_tomb : (size_t)i;
        }
        if (SLOT_TOMBSTONE == s->state) {
            if (SIZE_MAX == first_tomb) { first_tomb = (size_t)i; }
        } else if (0 == strcmp(s->key, key)) {
            return (size_t)i;
        }
        i = (i + 1) % t->capacity;
    }
    /* Unreachable when load factor maintained. */
    LOG_ERROR("Hash Table '%s' full - no insert slot", t->debug_name);
    assert(0 && "hash table full");
    return SIZE_MAX;
}

/* Insert key (already strdup'd, ownership transferred) into a freshly-sized
 * table that has no tombstones and known-free space. Used only by resize. */
static void raw_insert(HashTable* t, char* key_owned, void* value) {
    hash_t i = hash_string(key_owned) % t->capacity;
    while (SLOT_EMPTY != t->slots[i].state) {
        i = (i + 1) % t->capacity;
    }
    t->slots[i].key   = key_owned;
    t->slots[i].value = value;
    t->slots[i].state = SLOT_OCCUPIED;
}

static void resize(HashTable* t, size_t new_capacity) {
    assert(new_capacity > t->count);

    HashSlot* old_slots    = t->slots;
    size_t    old_capacity = t->capacity;

    t->slots      = calloc(new_capacity, sizeof(HashSlot));
    assert(t->slots);
    t->capacity   = new_capacity;
    t->tombstones = 0;
    /* t->count unchanged — same live entries, just rehomed */

    for (size_t i = 0; i < old_capacity; i++) {
        HashSlot* s = &old_slots[i];
        if (SLOT_OCCUPIED == s->state) {
            raw_insert(t, s->key, s->value);
        }
        /* tombstones and empties dropped on the floor */
    }
    free(old_slots);
}
