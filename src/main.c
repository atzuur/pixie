#include "cli.h"

int main(int argc, char** argv) {

    PXSettings s;
    int ret = px_settings_init(argc, argv, &s);
    if (ret) {
        px_settings_free(&s);
        return ret == PX_HELP_PRINTED ? 0 : ret;
    }

    return px_main(s);
}
