#ifndef TMK_PARSER_H
#define TMK_PARSER_H

#include "tmk_str.h"
#include "tmk_types.h"

// Fills ctx, which must be zeroed, from the build file at path
// Returns -1 and logs the error with its line number if the file is invalid.
// ctx must be freed with free_build_context in both cases
int parse_build_file(build_context* ctx, const tmk_str* path);

void free_build_context(build_context* ctx);

#endif
