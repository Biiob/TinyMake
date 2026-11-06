#ifndef TMK_PARSER_H
#define TMK_PARSER_H

#include "tmk_config.h"
#include "tmk_str.h"

// Fills config, which must be zeroed, from the build file at path
// Returns -1 and logs the error with its line number if the file is invalid.
// config must be freed with free_build_config in both cases
int parse_build_file(build_config* config, const tmk_str* path);

#endif
