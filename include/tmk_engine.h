#ifndef TMK_ENGINE_H
#define TMK_ENGINE_H

#include <stdbool.h>
#include <stddef.h>

#include "tmk_cmdlog.h"
#include "tmk_graph.h"

typedef struct
{
  tmk_cmdlog cmdlog;
  size_t max_jobs;
} tmk_engine_state;

// Builds the nodes of graph that are not up to date, and updates their status
// Returns false if one of them failed
bool tmk_engine_run(tmk_graph* graph, tmk_engine_state* state);

#endif
