#ifndef TMK_DEPFILE_H
#define TMK_DEPFILE_H

#include <stdbool.h>

#include "tmk_str.h"
#include "tmk_str_array.h"

// Reads the prerequisites of a make-style depfile, as written by gcc -MMD,
// skipping its target. deps is overwritten, so it must not own anything
// Returns false, leaving deps empty, if the file cannot be read
bool tmk_depfile_parse(tmk_str_array* deps, const tmk_str* path);

#endif
