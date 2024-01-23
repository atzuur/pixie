#include <pixie/log.h>
#include <pixie/util/map.h>
#include <pixie/util/utils.h>

#include <stdlib.h>
#include <string.h>
#include <errno.h>

int px_map_init(PXMap* map, const PXPair* elems, size_t len) {
    map->elems = malloc(sizeof *map->elems * len);
    if (!map->elems) {
        px_oom_msg(sizeof(PXPair) * len);
        return PXERROR(ENOMEM);
    }

    memcpy(map->elems, elems, sizeof *map->elems * len);
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

    return PXERROR(EINVAL);
}

int px_map_parse(PXMap* map, const char* str) {
    (void)map, (void)str;
    PX_LOG(PX_LOG_ERROR, "todo");
    return PXERROR(ENOSYS);
}

void px_map_free(PXMap* map) {
    if (!map)
        return;
    px_free(&map->elems);
    map->len = 0;
}
