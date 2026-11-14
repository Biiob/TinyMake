#include "tmk_engine.h"

#include <stdlib.h>
#include <sys/stat.h>

#include "tmk_cmdlog.h"
#include "tmk_depfile.h"
#include "tmk_fs.h"
#include "tmk_graph.h"
#include "tmk_log.h"
#include "tmk_path.h"
#include "tmk_runner.h"
#include "tmk_str.h"
#include "tmk_str_array.h"

// The files node is built from: its inputs in the graph, plus the headers its
// depfile lists. Returns false if the depfile could not be read
static bool collect_input_paths(tmk_str_array* paths, const tmk_graph* graph,
                                const tmk_node* node)
{
  *paths = (tmk_str_array){0};
  if (node->depfile_path.len > 0 &&
      !tmk_depfile_parse(paths, &node->depfile_path))
  {
    tmk_log_debug("Could not read dependency file: %s",
                  tmk_str_cstr(&node->depfile_path));
    return false;
  }

  for (size_t i = 0; i < node->inputs.len; ++i)
  {
    tmk_array_push(paths,
                   tmk_str_clone(&graph->items[node->inputs.items[i]].output));
  }

  return true;
}

// True if one of paths is missing, or newer than the output
static bool any_newer(const tmk_str_array* paths, const struct stat* out_stat)
{
  for (size_t i = 0; i < paths->len; ++i)
  {
    struct stat in_stat;
    if (stat(tmk_str_cstr(&paths->items[i]), &in_stat) != 0)
    {
      tmk_log_debug("Could not find or access the input file: %s",
                    tmk_str_cstr(&paths->items[i]));
      return true;
    }
    if (tmk_fs_is_newer(&in_stat, out_stat))
    {
      return true;
    }
  }

  return false;
}

// The output of node is up to date if it exists, is newer than all of its
// inputs, and was built by the same command
static bool up_to_date(const tmk_graph* graph, const tmk_node* node,
                       const tmk_cmdlog* cmdlog)
{
  struct stat out_stat;
  if (stat(tmk_str_cstr(&node->output), &out_stat) != 0)
  {
    return false;
  }

  tmk_str_array inputs;
  bool fresh = collect_input_paths(&inputs, graph, node) &&
               !any_newer(&inputs, &out_stat);
  tmk_str_array_free(&inputs);
  if (!fresh)
  {
    return false;
  }

  const tmk_str* recorded = tmk_cmdlog_find(cmdlog, &node->output);
  return recorded != NULL && tmk_str_equal(&node->command_display, recorded);
}

// Marks the node done, and makes ready the dependents that were only waiting
// for it
static void complete_node(tmk_graph* graph, size_t idx)
{
  tmk_node* node = &graph->items[idx];
  node->status = TMK_NODE_DONE;

  for (size_t i = 0; i < node->dependents.len; ++i)
  {
    tmk_node* dependent = &graph->items[node->dependents.items[i]];
    dependent->n_pending_inputs--;
    if (dependent->n_pending_inputs == 0 &&
        dependent->status == TMK_NODE_WAITING)
    {
      dependent->status = TMK_NODE_READY;
    }
  }
}

// Fails the node and, transitively, everything that depends on it
static void fail_node(tmk_graph* graph, size_t idx)
{
  tmk_node* node = &graph->items[idx];
  if (node->status == TMK_NODE_FAILED)
  {
    return;
  }

  node->status = TMK_NODE_FAILED;
  for (size_t i = 0; i < node->dependents.len; ++i)
  {
    fail_node(graph, node->dependents.items[i]);
  }
}

static bool create_output_dir(const tmk_node* node)
{
  tmk_str dir = tmk_path_get_dir(&node->output);
  bool ok = tmk_fs_create_dir(&dir);
  tmk_str_free(&dir);

  return ok;
}

// Adds a command to commands for each ready node that is not up to date
// The others are completed, or failed if their directory cannot be created
static void collect_ready(tmk_graph* graph, const tmk_cmdlog* cmdlog,
                          tmk_runner_command_array* commands)
{
  for (size_t i = 0; i < graph->len; ++i)
  {
    tmk_node* node = &graph->items[i];
    if (node->status != TMK_NODE_READY)
    {
      continue;
    }

    if (up_to_date(graph, node, cmdlog))
    {
      tmk_log_info("Skipping build of: %s", tmk_str_cstr(&node->output));
      complete_node(graph, i);
      continue;
    }

    if (!create_output_dir(node))
    {
      fail_node(graph, i);
      continue;
    }

    tmk_runner_command cmd = {
        .argv = node->command_argv,
        .display = &node->command_display,
        .name = &node->output,
        .id = i,
    };
    tmk_array_push(commands, cmd);
  }
}

// Updates the nodes of commands from their result, and records the commands
// that succeeded. A command that was not run counts as failed
static void apply_results(tmk_graph* graph, tmk_cmdlog* cmdlog,
                          const tmk_runner_command_array* commands)
{
  for (size_t i = 0; i < commands->len; ++i)
  {
    size_t idx = commands->items[i].id;
    if (commands->items[i].status == TMK_RUNNER_STATUS_SUCCEEDED)
    {
      const tmk_node* node = &graph->items[idx];
      tmk_cmdlog_record(cmdlog, &node->output, &node->command_display);
      complete_node(graph, idx);
    }
    else
    {
      fail_node(graph, idx);
    }
  }
}

static bool has_status(const tmk_graph* graph, tmk_node_status status)
{
  for (size_t i = 0; i < graph->len; ++i)
  {
    if (graph->items[i].status == status)
    {
      return true;
    }
  }

  return false;
}

// Nodes left to build. Every node ends up done or failed: failing a node
// fails its dependents, and completing it makes them ready once nothing else
// is pending
static bool has_unfinished(const tmk_graph* graph)
{
  return has_status(graph, TMK_NODE_READY) ||
         has_status(graph, TMK_NODE_WAITING);
}

bool tmk_engine_run(tmk_graph* graph, tmk_engine_state* state)
{
  while (has_unfinished(graph))
  {
    tmk_runner_command_array commands = {0};
    collect_ready(graph, &state->cmdlog, &commands);
    tmk_runner_run(commands.items, commands.len, state->max_jobs);
    apply_results(graph, &state->cmdlog, &commands);
    tmk_array_free(&commands);
  }

  return !has_status(graph, TMK_NODE_FAILED);
}
