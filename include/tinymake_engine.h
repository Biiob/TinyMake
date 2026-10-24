#ifndef TINYMAKE_ENGINE
#define TINYMAKE_ENGINE

#include "tinymake_types.h"

int create_needed_directories();
int open_log_file();
int close_log_file();
int generate_object_files(build_context* ctx, target* target);
int link_object_files(build_context* ctx, target* target);
bool should_recompile(compile_file* file, const char* command);
bool should_relink(const char* command);

#endif
