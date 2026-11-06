#ifndef TMK_CONFIG_H
#define TMK_CONFIG_H

#include <stddef.h>

#include "tmk_constants.h"
#include "tmk_str.h"
#include "tmk_str_array.h"

// What the build file describes. The parser fills it, the engine only reads it

typedef struct
{
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
} target;

typedef struct
{
  tmk_str compiler;
  tmk_str_array compile_flags;
  tmk_str_array link_flags;
  tmk_str_array includes_dir;
  target targets[MAX_TARGETS];
  size_t targets_count;
} build_config;

void free_build_config(build_config* config);

#endif
