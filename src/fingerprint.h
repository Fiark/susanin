#ifndef SUSANIN_FINGERPRINT_H
#define SUSANIN_FINGERPRINT_H

#include <stddef.h>
#include <stdint.h>

uint64_t susanin_fnv1a64(const void *data, size_t len);
void susanin_fingerprint_hex(const char *text, char out[17]);

#endif
