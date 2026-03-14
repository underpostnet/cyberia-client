#ifndef HELPER_H
#define HELPER_H

// Simple string hash function (djb2)
static inline unsigned long hash_string(const char* str) {
    unsigned long hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

#endif
