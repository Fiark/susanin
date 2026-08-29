#include "fingerprint.h"

#include <stdio.h>
#include <string.h>

uint64_t susanin_fnv1a64(const void *data, size_t len) {
    const unsigned char *p = (const unsigned char *)data;
    uint64_t h = UINT64_C(14695981039346656037);
    for (size_t i = 0; i < len; ++i) {
        h ^= (uint64_t)p[i];
        h *= UINT64_C(1099511628211);
    }
    return h;
}

void susanin_fingerprint_hex(const char *text, char out[17]) {
    const char *s = text ? text : "";
    uint64_t h = susanin_fnv1a64(s, strlen(s));
    (void)snprintf(out, 17, "%016llx", (unsigned long long)h);
}
