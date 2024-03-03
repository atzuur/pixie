#pragma once

#include <pixie/util/strconv.h>

#include <stddef.h>

typedef struct PXPair {
    char* key;
    char* value;
} PXPair;

typedef struct PXMap {
    size_t len;
    PXPair* elems;
} PXMap;

int px_map_init(PXMap* map, const PXPair* elems, size_t len);
void px_map_free(PXMap* map);

int px_map_get_str(const PXMap* map, char** dest, const char* key);
int px_map_get_int(const PXMap* map, int* dest, const char* key);
int px_map_get_float(const PXMap* map, float* dest, const char* key);
int px_map_get_bool(const PXMap* map, bool* dest, const char* key);

int px_map_set(PXMap* map, const char* key, char* value);

int px_map_parse(PXMap* map, const char* str);
