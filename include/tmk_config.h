#ifndef TMK_CONFIG_H
#define TMK_CONFIG_H

#include <stddef.h>

#include "tmk_array.h"
#include "tmk_str.h"
#include "tmk_str_array.h"

typedef enum
{
  TMK_TARGET_BINARY,
  TMK_TARGET_LIBRARY
} tmk_target_type;

typedef struct
{
  tmk_target_type type;
  tmk_str name;
  tmk_str_array sources;
  // Names of the libraries the target depends on, as written
  tmk_str_array deps;
  // Where the target is declared in the build file
  size_t line_no;
} tmk_target;

tmk_define_array(tmk_target_array, tmk_target);

typedef struct
{
  tmk_str compiler;
  tmk_str archiver;
  tmk_str_array compile_flags;
  tmk_str_array link_flags;
  tmk_str_array include_dirs;
  tmk_target_array targets;
} tmk_config;

void tmk_config_free(tmk_config* config);

#endif
