#pragma once

#include <pixie/pixie.h>

#define PX_HELP_PRINTED 2

int px_parse_args(int argc, char** argv, PXSettings* s);
void px_print_info(const char* prog_name, bool full);
