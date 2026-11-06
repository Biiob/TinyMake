#ifndef TMK_CONFIG_H
#define TMK_CONFIG_H

#include <stddef.h>

#include "tmk_constants.h"
#include "tmk_str.h"
#include "tmk_str_array.h"

// What the build file describes: filled by the parser, then only read

typedef struct
{
  tmk_str c_path;
  tmk_str o_path;
  tmk_str d_path;
} tmk_source;

typedef enum
{
  TMK_TARGET_BINARY,
  TMK_TARGET_LIBRARY
} tmk_target_type;

typedef struct
{
  tmk_target_type type;
  // Where the target is built, e.g. "bin/app"
  tmk_str path;
  tmk_source sources[TMK_MAX_SOURCES];
  size_t n_sources;
} tmk_target;

typedef struct
{
  tmk_str compiler;
  tmk_str_array compile_flags;
  tmk_str_array link_flags;
  tmk_str_array include_dirs;
  tmk_target targets[TMK_MAX_TARGETS];
  size_t n_targets;
} tmk_config;

// Takes ownership of c_path. The object and depfile mirror its path under
// TMK_OBJ_DIR, so that sources with the same name do not collide
tmk_source tmk_source_create(tmk_str c_path);

void tmk_config_free(tmk_config* config);

#endif
