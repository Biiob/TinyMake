#include "tmk_engine.h"

#include <assert.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "tmk_cmdlog.h"
#include "tmk_depfile.h"
#include "tmk_fs.h"
#include "tmk_graph.h"
#include "tmk_log.h"
#include "tmk_path.h"
#include "tmk_runner.h"
#include "tmk_str.h"
#include "tmk_str_array.h"

typedef struct
{
  tmk_graph* graph;
  tmk_cmdlog* cmdlog;
  // Nodes to check then build
  tmk_index_array queue;
  size_t head;
  // Set by the first failure: no new node is started afterwards
  bool stopping;
} build;

// The files node is built from: its inputs in the graph, plus the discovered
// inputs its depfile lists. Returns false if the depfile could not be read
static bool collect_input_paths(tmk_str_array* paths, const tmk_graph* graph,
                                const tmk_node* node)
{
  *paths = (tmk_str_array){0};
  if (node->depfile_path.len > 0 &&
      !tmk_depfile_parse(paths, &node->depfile_path))
  {
    tmk_log_debug("Could not read depfile: %s",
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
// inputs and discovered inputs, and was built by the same command
static bool up_to_date(const tmk_graph* graph, const tmk_node* node,
                       const tmk_cmdlog* cmdlog)
{
  struct stat out_stat;
  if (stat(tmk_str_cstr(&node->output), &out_stat) != 0)
  {
    return false;
  }

  tmk_str_array paths;
  bool inputs_older = collect_input_paths(&paths, graph, node) &&
                      !any_newer(&paths, &out_stat);
  tmk_str_array_free(&paths);
  if (!inputs_older)
  {
    return false;
  }

  const tmk_str* recorded = tmk_cmdlog_find(cmdlog, &node->output);
  return recorded != NULL && tmk_str_equal(&node->command_display, recorded);
}

// Marks the node done, and queues the dependents that were only waiting for it
static void complete_node(build* b, size_t idx)
{
  tmk_node* node = &b->graph->items[idx];
  node->status = TMK_NODE_DONE;

  for (size_t i = 0; i < node->dependents.len; ++i)
  {
    size_t dependent_idx = node->dependents.items[i];
    tmk_node* dependent = &b->graph->items[dependent_idx];
    dependent->n_pending_inputs--;
    if (dependent->n_pending_inputs == 0 &&
        dependent->status == TMK_NODE_WAITING)
    {
      dependent->status = TMK_NODE_READY;
      tmk_array_push(&b->queue, dependent_idx);
    }
  }
}

// Fails the node and, transitively, everything that depends on it
static void fail_node(build* b, size_t idx)
{
  b->stopping = true;

  tmk_node* node = &b->graph->items[idx];
  if (node->status == TMK_NODE_FAILED)
  {
    return;
  }

  node->status = TMK_NODE_FAILED;
  for (size_t i = 0; i < node->dependents.len; ++i)
  {
    fail_node(b, node->dependents.items[i]);
  }
}

static bool create_output_dir(const tmk_node* node)
{
  tmk_str dir = tmk_path_get_dir(&node->output);
  bool ok = tmk_fs_create_dir(&dir);
  if (!ok)
  {
    tmk_log_error("Could not create directory %s: %s", tmk_str_cstr(&dir),
                  strerror(errno));
  }
  tmk_str_free(&dir);

  return ok;
}

// ar adds to an existing archive and never removes from it: without this, the
// object of a source taken out of a library would stay in it
static bool remove_old_output(const tmk_node* node)
{
  if (unlink(tmk_str_cstr(&node->output)) == 0 || errno == ENOENT)
  {
    return true;
  }

  tmk_log_error("Could not remove %s: %s", tmk_str_cstr(&node->output),
                strerror(errno));
  return false;
}

static bool start_node(tmk_runner* runner, const tmk_node* node, size_t idx)
{
  tmk_runner_command cmd = {
      .argv = node->command_argv,
      .display = &node->command_display,
      .name = &node->output,
      .id = idx,
  };

  return create_output_dir(node) && remove_old_output(node) &&
         tmk_runner_start(runner, &cmd);
}

// Takes nodes from the queue while the runner has room: completes the ones
// that are up to date, and starts the others
static void start_ready(build* b, tmk_runner* runner)
{
  while (!b->stopping && !tmk_runner_is_full(runner) && b->head < b->queue.len)
  {
    size_t idx = b->queue.items[b->head++];
    tmk_node* node = &b->graph->items[idx];

    if (up_to_date(b->graph, node, b->cmdlog))
    {
      tmk_log_info("Skipping build of: %s", tmk_str_cstr(&node->output));
      complete_node(b, idx);
    }
    else if (start_node(runner, node, idx))
    {
      node->status = TMK_NODE_RUNNING;
    }
    else
    {
      fail_node(b, idx);
    }
  }
}

// Records the command of a node that succeeded, and completes or fails it
static void apply_result(build* b, const tmk_runner_result* result)
{
  if (result->status == TMK_RUNNER_STATUS_SUCCEEDED)
  {
    const tmk_node* node = &b->graph->items[result->id];
    tmk_cmdlog_record(b->cmdlog, &node->output, &node->command_display);
    complete_node(b, result->id);
  }
  else
  {
    fail_node(b, result->id);
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

bool tmk_engine_run(tmk_graph* graph, tmk_engine_state* state)
{
  build b = {.graph = graph, .cmdlog = &state->cmdlog};
  for (size_t i = 0; i < graph->len; ++i)
  {
    if (graph->items[i].status == TMK_NODE_READY)
    {
      tmk_array_push(&b.queue, i);
    }
  }

  tmk_runner runner;
  tmk_runner_init(&runner, state->max_jobs);

  // After a failure, nothing new starts but what runs is still waited for
  for (;;)
  {
    start_ready(&b, &runner);
    if (tmk_runner_n_running(&runner) == 0)
    {
      break;
    }

    tmk_runner_result result;
    if (!tmk_runner_wait_any(&runner, &result))
    {
      break;
    }
    apply_result(&b, &result);
  }

  // Only left running if waiting failed: their results are unknown
  for (size_t i = 0; i < graph->len; ++i)
  {
    if (graph->items[i].status == TMK_NODE_RUNNING)
    {
      fail_node(&b, i);
    }
  }
  tmk_runner_free(&runner);
  tmk_array_free(&b.queue);

  assert(b.stopping || !has_status(graph, TMK_NODE_WAITING));
  assert(b.stopping || !has_status(graph, TMK_NODE_READY));

  return !has_status(graph, TMK_NODE_FAILED);
}
