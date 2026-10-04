#ifndef TINYMAKE_ENGINE
#define TINYMAKE_ENGINE

#include "tinymake_types.h"

int generate_object_files(build_context_t* ctx);
int link_object_files(build_context_t* ctx);
bool should_recompile(compile_file_t* file);

#endif
