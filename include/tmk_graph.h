#ifndef TMK_GRAPH_H
#define TMK_GRAPH_H

#include <stdbool.h>
#include <stddef.h>

#include "tmk_array.h"
#include "tmk_config.h"
#include "tmk_str.h"

tmk_define_array(tmk_index_array, size_t);

typedef enum
{
  // Some inputs are not built yet
  TMK_NODE_WAITING,
  // All inputs are built: can be checked, then built if needed
  TMK_NODE_READY,
  TMK_NODE_RUNNING,
  // Built, up to date, or a source file
  TMK_NODE_DONE,
  // Its command failed, or one of its inputs did
  TMK_NODE_FAILED
} tmk_node_status;

// A file of the build, and how to make it
typedef struct
{
  tmk_str output;
  // Indices of the nodes it is built from
  tmk_index_array inputs;
  // Indices of the nodes built from it
  tmk_index_array dependents;
  // NULL for a source. The node owns the array, not the arguments
  char** command_argv;
  // The arguments joined, as shown and recorded in the command log
  tmk_str command_display;
  // Empty if the command writes no depfile
  tmk_str depfile_path;
  tmk_node_status status;
  // Inputs that are not done yet: the node is ready when this reaches 0
  size_t n_pending_inputs;
} tmk_node;

// Every file of the build, as nodes that refer to each other by index
tmk_define_array(tmk_graph, tmk_node);

// Adds the nodes needed to build the targets of config to graph
// The commands point into config, which must outlive the graph
bool tmk_graph_build(tmk_graph* graph, const tmk_config* config);

void tmk_graph_free(tmk_graph* graph);

#endif
