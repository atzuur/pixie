#include <pixie/util/map.h>
#include <pixie/log.h>
#include <pixie/util/utils.h>
#include <assert.h>
#include <errno.h>

int main(void) {
    PXMap map;
    const char* test_success = "foo=hello:bar=5:baz=3.14:qux=true";
    int ret = px_map_parse(&map, test_success);
    assert(ret == 0);
    assert(map.len == 4);

    char* foo;
    ret = px_map_get_str(&map, &foo, "foo");
    assert(ret == 0);
    assert(strcmp(foo, "hello") == 0);

    int bar;
    ret = px_map_get_int(&map, &bar, "bar");
    assert(ret == 0);
    assert(bar == 5);

    float baz;
    ret = px_map_get_float(&map, &baz, "baz");
    assert(ret == 0);
    assert(baz == 3.14f);

    bool qux;
    ret = px_map_get_bool(&map, &qux, "qux");
    assert(ret == 0);
    assert(qux == true);

    px_map_free(&map);

    const char* test_missing_eq = "qux";
    ret = px_map_parse(&map, test_missing_eq);
    assert(ret == PXERROR(EINVAL));

    const char* test_missing_key = "=qux";
    ret = px_map_parse(&map, test_missing_key);
    assert(ret == PXERROR(EINVAL));

    const char* test_missing_value = "qux=";
    ret = px_map_parse(&map, test_missing_value);
    assert(ret == PXERROR(EINVAL));
}
