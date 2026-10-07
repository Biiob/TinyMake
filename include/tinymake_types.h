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

typedef enum { BINARY, LIBRARY } target_type_t;

typedef struct
{
    target_type_t type;
    char name[MAX_FILE_NAME_SIZE];
    compile_file_t source_files[MAX_C_FILES];
    size_t source_files_count;
    bool should_relink;
} target_t;

typedef struct
{
    char compiler[32];
    char compile_flags[MAX_LINE_SIZE];
    char includes[MAX_INCLUDE_DIRECTORIES][MAX_FILE_NAME_SIZE];
    size_t includes_count;
    target_t targets[MAX_TARGETS];
    size_t targets_count;
} build_context_t;

#endif
