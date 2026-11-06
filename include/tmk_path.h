#ifndef TMK_PATH_H
#define TMK_PATH_H

#include "tmk_str.h"

// Both return a new string

// "src/a.c" gives "src/a". A dot in a directory name is not an extension
tmk_str tmk_path_without_ext(const tmk_str* path);

// "src/a.c" gives "src", and "a.c" gives ""
tmk_str tmk_path_get_dir(const tmk_str* path);

#endif
