#ifndef TINYMAKE_ENGINE
#define TINYMAKE_ENGINE

#include "tinymake_types.h"

int create_needed_directories();
int generate_object_files(build_context_t* ctx);
int link_object_files(build_context_t* ctx);
bool should_recompile(compile_file_t* file, const char* command);

#endif
