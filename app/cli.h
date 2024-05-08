#pragma once

#include "app.h"

#define HELP_PRINTED PXERROR(2)

int parse_args(int argc, char** argv, Settings* s);
void parsed_args_free(Settings* s);

void print_info(const char* prog_name, bool full);
