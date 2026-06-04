#include "hash_table.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "util/log.h"

/* Resize when count exceeds this fraction of capacity. */
#define HASH_LOAD_NUM 7
#define HASH_LOAD_DEN 10

static size_t find_index(const HashTable* t, const char* key, hash_t h);
static void   raw_insert(HashTable* t, char* key_owned, void* value, hash_t h);
static void   resize(HashTable* t, size_t new_capacity);

void hash_table_init(HashTable* t, size_t initial_capacity, HashFreeFn free_fn, const char* debug_name) {
    assert(t);
    assert(initial_capacity > 0);
    assert(free_fn);
    assert(debug_name);
    t->slots      = calloc(initial_capacity, sizeof(HashSlot));
    assert(t->slots);
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
    size_t i = find_index(t, key, hash_string(key));
    return (SIZE_MAX == i) ? NULL : t->slots[i].value;
}

bool hash_table_contains(const HashTable* t, const char* key) {
    assert(t);
    assert(key);
    return SIZE_MAX != find_index(t, key, hash_string(key));
}

void hash_table_put(HashTable* t, const char* key, void* value) {
    assert(t);
    assert(key);
    assert(value);

    hash_t h = hash_string(key);

    /* Replace in place if the key already exists — no count change. */
    size_t found = find_index(t, key, h);
    if (SIZE_MAX != found) {
        HashSlot* s = &t->slots[found];
        if (s->value != value && t->free_fn) { t->free_fn(s->value); }
        s->value = value;
        return;
    }

    /* Grow before insert so an EMPTY slot is guaranteed (loop terminates). */
    if ((t->count + 1) * HASH_LOAD_DEN > t->capacity * HASH_LOAD_NUM) {
        size_t new_cap = t->capacity * 2;
        if (new_cap < t->capacity) { new_cap = t->capacity; } /* guard size_t wrap */
        resize(t, new_cap);
    }

    char* key_owned = strdup(key);
    assert(key_owned);
    raw_insert(t, key_owned, value, h);
}

bool hash_table_remove(HashTable* t, const char* key) {
    assert(t);
    assert(key);

    size_t i = find_index(t, key, hash_string(key));
    if (SIZE_MAX == i) { return false; }

    HashSlot* s = &t->slots[i];
    if (t->free_fn && s->value) { t->free_fn(s->value); }
    free(s->key);

    /* Backward-shift: pull each following entry back one slot until we hit an
     * EMPTY slot or an entry already at its home (probe distance 0). This
     * keeps the table tombstone-free and preserves the probe invariant. */
    for (;;) {
        size_t next = (i + 1) % t->capacity;
        HashSlot* ns = &t->slots[next];
        if (SLOT_OCCUPIED != ns->state) { break; }
        size_t home = ns->hash % t->capacity;
        if (next == home) { break; }
        t->slots[i] = *ns;
        i = next;
    }
    t->slots[i].state = SLOT_EMPTY;
    t->slots[i].key   = NULL;
    t->slots[i].value = NULL;
    t->slots[i].hash  = 0;
    t->count--;
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
    if (0 == t->count) { return 0; }

    /* Snapshot matching keys first, then remove by key. Backward-shift moves
     * surviving entries between slots, so iterate-and-remove-in-place is
     * unsafe; removing by key content re-locates each entry correctly. */
    char** keys = malloc(sizeof(char*) * t->count);
    assert(keys);
    size_t n = 0;
    for (size_t i = 0; i < t->capacity; i++) {
        HashSlot* s = &t->slots[i];
        if (SLOT_OCCUPIED == s->state && pred(s->key, s->value, user_data)) {
            keys[n++] = strdup(s->key);
            assert(keys[n - 1]);
        }
    }
    for (size_t i = 0; i < n; i++) {
        hash_table_remove(t, keys[i]);
        free(keys[i]);
    }
    free(keys);
    return n;
}

/* ── Internals ──────────────────────────────────────────────────────── */

/* Locate the slot holding `key`, or SIZE_MAX. Robin Hood early-out: once the
 * scanned probe distance exceeds the stored entry's own distance, the key
 * cannot be present (it would have stolen this slot on insert). */
static size_t find_index(const HashTable* t, const char* key, hash_t h) {
    size_t i    = h % t->capacity;
    size_t dist = 0;
    for (;;) {
        const HashSlot* s = &t->slots[i];
        if (SLOT_EMPTY == s->state) { return SIZE_MAX; }
        size_t home  = s->hash % t->capacity;
        size_t edist = (i + t->capacity - home) % t->capacity;
        if (edist < dist) { return SIZE_MAX; }
        if (s->hash == h && 0 == strcmp(s->key, key)) { return i; }
        i = (i + 1) % t->capacity;
        dist++;
        if (dist >= t->capacity) { return SIZE_MAX; } /* full-table safety */
    }
}

/* Insert a unique key (ownership transferred) via Robin Hood displacement.
 * Caller guarantees the key is absent and an EMPTY slot exists. */
static void raw_insert(HashTable* t, char* key_owned, void* value, hash_t h) {
    size_t i    = h % t->capacity;
    size_t dist = 0;
    char*  ik   = key_owned;
    void*  iv   = value;
    hash_t ih   = h;
    for (;;) {
        HashSlot* s = &t->slots[i];
        if (SLOT_OCCUPIED != s->state) {
            s->key   = ik;
            s->value = iv;
            s->hash  = ih;
            s->state = SLOT_OCCUPIED;
            t->count++;
            return;
        }
        size_t home  = s->hash % t->capacity;
        size_t edist = (i + t->capacity - home) % t->capacity;
        if (edist < dist) {
            char*  tk = s->key;  void* tv = s->value;  hash_t th = s->hash;
            s->key = ik;  s->value = iv;  s->hash = ih;
            ik = tk;  iv = tv;  ih = th;
            dist = edist;
        }
        i = (i + 1) % t->capacity;
        dist++;
    }
}

static void resize(HashTable* t, size_t new_capacity) {
    assert(new_capacity > t->count);

    HashSlot* old_slots    = t->slots;
    size_t    old_capacity = t->capacity;

    t->slots      = calloc(new_capacity, sizeof(HashSlot));
    assert(t->slots);
    t->capacity   = new_capacity;
    t->count      = 0;
    t->tombstones = 0;

    for (size_t i = 0; i < old_capacity; i++) {
        HashSlot* s = &old_slots[i];
        if (SLOT_OCCUPIED == s->state) {
            raw_insert(t, s->key, s->value, s->hash);
        }
    }
    free(old_slots);
}
