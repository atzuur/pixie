#include <pixie/log.h>
#include <pixie/util/map.h>
#include <pixie/util/utils.h>

#include <stdlib.h>
#include <string.h>
#include <errno.h>

int px_map_init(PXMap* map, const PXPair* elems, size_t len) {
    size_t elems_size = sizeof *map->elems * len;
    map->elems = malloc(elems_size);
    if (!map->elems) {
        px_oom_msg(elems_size);
        return PXERROR(ENOMEM);
    }

    map->len = len;
    for (size_t i = 0; i < len; i++) {
        map->elems[i].key = elems ? strdup(elems[i].key) : NULL;
        map->elems[i].value = elems ? strdup(elems[i].value) : NULL;
    }

    return 0;
}

int px_map_get_str(const PXMap* map, char** dest, const char* key) {
    for (size_t i = 0; i < map->len; i++) {
        const PXPair* pair = &map->elems[i];
        if (strcmp(pair->key, key) == 0) {
            *dest = pair->value;
            return 0;
        }
    }

    return PXERROR(ENOENT);
}

int px_map_get_int(const PXMap* map, int* dest, const char* key) {
    for (size_t i = 0; i < map->len; i++) {
        const PXPair* pair = &map->elems[i];
        if (strcmp(pair->key, key) == 0)
            return px_strtoi(dest, pair->value);
    }

    return PXERROR(ENOENT);
}

int px_map_get_float(const PXMap* map, float* dest, const char* key) {
    for (size_t i = 0; i < map->len; i++) {
        const PXPair* pair = &map->elems[i];
        if (strcmp(pair->key, key) == 0)
            return px_strtof(dest, pair->value);
    }

    return PXERROR(ENOENT);
}

int px_map_get_bool(const PXMap* map, bool* dest, const char* key) {
    for (size_t i = 0; i < map->len; i++) {
        const PXPair* pair = &map->elems[i];
        if (strcmp(pair->key, key) == 0)
            return px_strtob(dest, pair->value);
    }

    return PXERROR(ENOENT);
}

int px_map_set(PXMap* map, const char* key, char* value) {
    for (size_t i = 0; i < map->len; i++) {
        PXPair* pair = &map->elems[i];
        if (strcmp(pair->key, key) == 0) {
            pair->value = value;
            return 0;
        }
    }

    return PXERROR(EINVAL);
}

static int parse_pair(PXPair* dest, const char* str_begin, const char* str_end) {
    assert(dest);
    assert(str_begin);
    assert(str_end);

    // int is needed for printing with %.*s
    int str_len = (int)(str_end - str_begin);

    const char* equals = strchr(str_begin, '=');
    if (!equals) {
        px_log(PX_LOG_ERROR, "No \"=\" found in key-value pair \"%.*s\"\n", str_len, str_begin);
        return PXERROR(EINVAL);
    }

    size_t key_len = (size_t)(equals - str_begin);
    if (key_len == 0) {
        px_log(PX_LOG_ERROR, "Empty key in key-value pair \"%.*s\"\n", str_len, str_begin);
        return PXERROR(EINVAL);
    }
    dest->key = strndup(str_begin, key_len);
    if (!dest->key) {
        px_oom_msg(key_len);
        return PXERROR(ENOMEM);
    }

    size_t value_len = (size_t)(str_end - equals - 1);
    if (value_len == 0) {
        px_log(PX_LOG_ERROR, "Empty value in key-value pair \"%.*s\"\n", str_len, str_begin);
        return PXERROR(EINVAL);
    }
    dest->value = strndup(equals + 1, value_len);
    if (!dest->value) {
        px_free(&dest->key);
        px_oom_msg(value_len);
        return PXERROR(ENOMEM);
    }

    return 0;
}

int px_map_parse(PXMap* map, const char* str) {
    assert(str);
    assert(map);

    // TODO: support intermediary whitespace

    if (!*str) {
        map->len = 0;
        map->elems = NULL;
        return 0;
    }

    size_t len = 1;
    for (const char* it = str; *it; it++) {
        if (*it == ':')
            len++;
    }
    int ret = px_map_init(map, NULL, len);
    if (ret < 0)
        return ret;

    size_t pair_idx = 0;
    for (const char* it = str;; it++) {
        if (*it == ':' || !*it) {
            // TODO: handle duplicate keys
            ret = parse_pair(&map->elems[pair_idx++], str, it);
            if (ret < 0) {
                px_map_free(map);
                return ret;
            }
            if (!*it)
                break;

            str = it + 1;
        }
    }

    return 0;
}

void px_map_free(PXMap* map) {
    if (!map)
        return;

    for (size_t i = 0; i < map->len; i++) {
        px_free(&map->elems[i].key);
        px_free(&map->elems[i].value);
    }
    px_free(&map->elems);

    map->len = 0;
}
