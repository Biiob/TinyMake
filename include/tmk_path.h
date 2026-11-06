#ifndef TMK_PATH_H
#define TMK_PATH_H

#include "tmk_str.h"
#include "tmk_types.h"

tmk_str tmk_path_without_ext(const tmk_str* p);
tmk_str tmk_path_get_dir(const tmk_str* p);

compile_file create_compile_file(tmk_str* c_path);

#endif
