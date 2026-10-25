#ifndef TMK_TYPES_H
#define TMK_TYPES_H

#include <stdbool.h>
#include <stdlib.h>

#include "tmk_constants.h"

typedef struct
{
  size_t n_processes;
} command_line_arguments;

typedef struct
{
  char stripped_name[MAX_FILE_NAME_SIZE];
  char c_path[MAX_PATH_NAME_SIZE];
  char o_path[MAX_PATH_NAME_SIZE];
  char d_path[MAX_PATH_NAME_SIZE];
} compile_file;

typedef enum
{
  BINARY,
  LIBRARY
} target_type;

typedef struct
{
  target_type type;
  char name[MAX_FILE_NAME_SIZE];
  compile_file source_files[MAX_C_FILES];
  size_t source_files_count;
  bool should_relink;
} target;

typedef struct
{
  char compiler[32];
  char compile_flags[MAX_FLAGS][MAX_FLAG_SIZE];
  size_t compile_flags_count;
  char link_flags[MAX_FLAGS][MAX_FLAG_SIZE];
  size_t link_flags_count;
  char includes_dir[MAX_INCLUDE_DIRECTORIES][MAX_FILE_NAME_SIZE];
  size_t includes_dir_count;
  target targets[MAX_TARGETS];
  size_t targets_count;
  int n_processes;
} build_context;

#endif
