#ifndef TMK_DEPFILE_H
#define TMK_DEPFILE_H

#include <stdbool.h>

#include "tmk_str.h"
#include "tmk_str_array.h"

// Reads the discovered inputs of a make-style depfile, as written by gcc -MMD,
// skipping the rule's target. paths is overwritten, so it must not own anything
// Returns false, leaving paths empty, if the file cannot be read
bool tmk_depfile_parse(tmk_str_array* paths, const tmk_str* path);

#endif
