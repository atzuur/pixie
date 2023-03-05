#include "pixie.h"

int main(int argc, char** argv) {

    PXSettings s;
    int ret = px_init_settings(argc, argv, &s);
    if (ret) {
        px_free_settings(&s);
        return ret == PX_HELP_PRINTED ? 0 : ret;
    }

    return px_main(s);
}
