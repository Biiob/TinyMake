#ifndef TINYMAKE_TYPES
#define TINYMAKE_TYPES

#include <stdbool.h>
#include <stdlib.h>

#include "tinymake_constants.h"

typedef struct
{
    char stripped_name[MAX_FILE_NAME_SIZE];
    char c_path[MAX_PATH_NAME_SIZE];
    char o_path[MAX_PATH_NAME_SIZE];
    char d_path[MAX_PATH_NAME_SIZE];
} compile_file_t;

typedef struct
{
    char compiler[32];
    char compile_flags[MAX_LINE_SIZE];
    char includes[MAX_INCLUDE_DIRECTORIES][MAX_FILE_NAME_SIZE];
    size_t includes_count;
    char binary_name[MAX_FILE_NAME_SIZE];
    compile_file_t compile_files[MAX_C_FILES];
    size_t compile_files_count;
    bool has_new_obj_file;
} build_context_t;

#endif
