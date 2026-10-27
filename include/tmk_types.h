#ifndef TMK_TYPES_H
#define TMK_TYPES_H

#include <stdbool.h>
#include <stdlib.h>

#include "tmk_constants.h"
#include "tmk_str.h"

typedef struct
{
  size_t n_processes;
} command_line_arguments;

typedef struct
{
  tmk_str stripped_name;
  tmk_str c_path;
  tmk_str o_path;
  tmk_str d_path;
} compile_file;

typedef enum
{
  BINARY,
  LIBRARY
} target_type;

typedef struct
{
  target_type type;
  tmk_str name;
  compile_file source_files[MAX_C_FILES];
  size_t source_files_count;
  bool should_relink;
} target;

typedef struct
{
  tmk_str compiler;
  tmk_str compile_flags[MAX_FLAGS];
  size_t compile_flags_count;
  tmk_str link_flags[MAX_FLAGS];
  size_t link_flags_count;
  tmk_str includes_dir[MAX_INCLUDE_DIRECTORIES];
  size_t includes_dir_count;
  target targets[MAX_TARGETS];
  size_t targets_count;
  int n_processes;
} build_context;

#endif
