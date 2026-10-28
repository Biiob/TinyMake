#ifndef TMK_DEPFILE_H
#define TMK_DEPFILE_H

#include <sys/types.h>

#include "tmk_str.h"
#include "tmk_str_array.h"

// Reads the prerequisites of a make-style depfile (as written by gcc -MMD),
// skipping the targets
// deps is overwritten, so it must not hold anything that needs freeing
// Returns the number of prerequisites, or -1 if the file cannot be read (deps
// is then left empty)
ssize_t parse_depfile(tmk_str_array* deps, const tmk_str* path);

#endif
