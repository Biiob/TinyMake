#ifndef TMK_ENGINE_H
#define TMK_ENGINE_H

#include <stddef.h>

#include "tmk_cmdlog.h"
#include "tmk_config.h"

// What a build needs besides its configuration, and what it changes
typedef struct
{
  // Commands that succeeded are recorded here, even when the build fails
  tmk_cmdlog cmdlog;
  // Maximum number of commands run at the same time, never 0
  size_t n_processes;
} build_state;

int generate_object_files(const build_config* config, build_state* state,
                          const target* target);
int link_object_files(const build_config* config, build_state* state,
                      const target* target);

#endif
