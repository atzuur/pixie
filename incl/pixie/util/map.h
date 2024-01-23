#pragma once

#include <stddef.h>

typedef struct PXPair {
    const char* key;
    const char* value;
} PXPair;

typedef struct PXMap {
    size_t len;
    PXPair* elems;
} PXMap;

int px_map_init(PXMap* map, const PXPair* elems, size_t len);
void px_map_free(PXMap* map);

const char* px_map_get(const PXMap* map, const char* key);
int px_map_set(PXMap* map, const char* key, const char* value);

int px_map_parse(PXMap* map, const char* str);
