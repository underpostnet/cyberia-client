#include "util/utils.h"

#include <assert.h>
#include <string.h>

void copy_str(char* dst, size_t cap, const char* src) {
    assert(dst && cap > 0);
    if (!src) { dst[0] = '\0'; return; }
    strncpy(dst, src, cap - 1);
    dst[cap - 1] = '\0';
}

void* pool_take(void* base, size_t elem_size, int count,
                float (*rank)(const void* elem)) {
    assert(base && rank && count > 0);

    char* slots = (char*)base;
    char* best  = slots;
    float best_rank = rank(slots);

    for (int i = 1; i < count; i++) {
        char* elem = slots + (size_t)i * elem_size;
        float r = rank(elem);
        if (r > best_rank) {
            best_rank = r;
            best = elem;
        }
    }
    memset(best, 0, elem_size);
    return best;
}

const cJSON* envelope_success_doc(const cJSON* root) {
    if (!root) return NULL;
    const cJSON* status = cJSON_GetObjectItemCaseSensitive(root, "status");
    const cJSON* doc    = cJSON_GetObjectItemCaseSensitive(root, "data");
    if (!cJSON_IsString(status) || 0 != strcmp(status->valuestring, "success") ||
        !cJSON_IsObject(doc)) {
        return NULL;
    }
    return doc;
}
