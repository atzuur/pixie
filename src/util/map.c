#include "utils.h"

#include <pixie/util/map.h>

#include <stdlib.h>
#include <string.h>

int px_map_init(PXMap* map, const PXPair* elems, size_t len) {
    map->elems = malloc(sizeof(PXPair) * len);
    if (!map->elems) {
        oom(sizeof(PXPair) * len);
        return -1;
    }

    memcpy(map->elems, elems, sizeof(PXPair) * len);
    map->len = len;

    return 0;
}

const char* px_map_get(const PXMap* map, const char* key) {
    for (size_t i = 0; i < map->len; i++) {
        const PXPair* pair = &map->elems[i];
        if (strcmp(pair->key, key) == 0) {
            return pair->value;
        }
    }

    return NULL;
}

int px_map_set(PXMap* map, const char* key, const char* value) {
    for (size_t i = 0; i < map->len; i++) {
        PXPair* pair = &map->elems[i];
        if (strcmp(pair->key, key) == 0) {
            pair->value = value;
            return 0;
        }
    }

    return -1;
}
