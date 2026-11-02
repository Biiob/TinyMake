#ifndef TMK_PATH_H
#define TMK_PATH_H

#include "tmk_str.h"
#include "tmk_types.h"

tmk_str get_file_name_without_ext(const tmk_str* full_path);
compile_file create_compile_file(tmk_str* c_path);

#endif
