#ifndef TMK_PARSER_H
#define TMK_PARSER_H

#include <stdbool.h>

#include "tmk_config.h"
#include "tmk_str.h"

// Fills config, which must be zeroed, from the build file at path. Logs the
// error with its line number and returns false if the file is invalid.
// config must be freed with tmk_config_free in both cases
bool tmk_parser_parse(tmk_config* config, const tmk_str* path);

#endif
