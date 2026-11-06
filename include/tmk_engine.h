#ifndef TMK_ENGINE_H
#define TMK_ENGINE_H

#include <stdbool.h>
#include <stddef.h>

#include "tmk_cmdlog.h"
#include "tmk_config.h"

// What a build needs besides its configuration, and what it changes
typedef struct
{
  // Gets the commands that succeeded, even when the build fails
  tmk_cmdlog cmdlog;
  // At least 1
  size_t max_jobs;
} tmk_engine_state;

// Compiles the sources of target that are not up to date
// Returns false if one of them failed
bool tmk_engine_compile(const tmk_config* config, tmk_engine_state* state,
                        const tmk_target* target);

// Links target if it is not up to date. Returns false if linking failed
bool tmk_engine_link(const tmk_config* config, tmk_engine_state* state,
                     const tmk_target* target);

#endif
