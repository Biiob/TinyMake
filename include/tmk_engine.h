#ifndef TMK_ENGINE_H
#define TMK_ENGINE_H

#include "tmk_types.h"

int create_needed_directories();
int open_log_file();
int close_log_file();
int generate_object_files(build_context* ctx, target* target);
int link_object_files(build_context* ctx, target* target);

#endif
