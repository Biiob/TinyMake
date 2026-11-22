#include "tmk_graph.h"

#include <stdarg.h>
#include <stdlib.h>

#include "tmk_constants.h"
#include "tmk_log.h"
#include "tmk_mem.h"
#include "tmk_path.h"
#include "tmk_str.h"

// Returns the index of the node that produces output, or graph->len if none
static size_t find_node(const tmk_graph* graph, const tmk_str* output)
{
  for (size_t i = 0; i < graph->len; ++i)
  {
    if (tmk_str_equal(&graph->items[i].output, output))
    {
      return i;
    }
  }

  return graph->len;
}

// Moves node into the graph and returns its index
static size_t add_node(tmk_graph* graph, tmk_node node)
{
  size_t idx = graph->len;
  tmk_array_push(graph, node);
  return idx;
}

// Records that the node at output_idx is built from the one at input_idx
static void add_edge(tmk_graph* graph, size_t input_idx, size_t output_idx)
{
  tmk_node* input = &graph->items[input_idx];
  tmk_node* output = &graph->items[output_idx];

  tmk_array_push(&input->dependents, output_idx);
  tmk_array_push(&output->inputs, input_idx);
  if (input->status != TMK_NODE_DONE)
  {
    output->n_pending_inputs++;
  }
}

// The arguments point into config and the graph, which must outlive the result
static char** build_object_argv(const tmk_graph* graph,
                                const tmk_config* config, const tmk_node* node)
{
  // Compiler, flags, "-I <dir>" pairs, 8 fixed arguments and NULL
  size_t argc = config->compile_flags.len + 2 * config->include_dirs.len + 10;
  char** argv = tmk_xrealloc(NULL, argc * sizeof(*argv));

  size_t arg = 0;
  argv[arg++] = config->compiler.str;
  for (size_t i = 0; i < config->compile_flags.len; ++i)
  {
    argv[arg++] = config->compile_flags.items[i].str;
  }
  for (size_t i = 0; i < config->include_dirs.len; ++i)
  {
    argv[arg++] = "-I";
    argv[arg++] = config->include_dirs.items[i].str;
  }
  argv[arg++] = "-fdiagnostics-color=always";
  argv[arg++] = "-MMD";
  argv[arg++] = "-MF";
  argv[arg++] = node->depfile_path.str;
  argv[arg++] = "-c";
  argv[arg++] = graph->items[node->inputs.items[0]].output.str;
  argv[arg++] = "-o";
  argv[arg++] = node->output.str;
  argv[arg] = NULL;

  return argv;
}

// The arguments point into config and the graph, which must outlive the result
static char** build_binary_argv(const tmk_graph* graph,
                                const tmk_config* config, const tmk_node* node)
{
  // Compiler, flags, "-o <path>", objects and NULL
  size_t argc = config->link_flags.len + node->inputs.len + 4;
  char** argv = tmk_xrealloc(NULL, argc * sizeof(*argv));

  size_t arg = 0;
  argv[arg++] = config->compiler.str;
  for (size_t i = 0; i < config->link_flags.len; ++i)
  {
    argv[arg++] = config->link_flags.items[i].str;
  }
  argv[arg++] = "-o";
  argv[arg++] = node->output.str;
  for (size_t i = 0; i < node->inputs.len; ++i)
  {
    argv[arg++] = graph->items[node->inputs.items[i]].output.str;
  }
  argv[arg] = NULL;

  return argv;
}

// "ar rcs <library> <objects...>"
// The arguments point into config and the graph, which must outlive the result
static char** build_archive_argv(const tmk_graph* graph,
                                 const tmk_config* config, const tmk_node* node)
{
  // Archiver, "rcs", the library, objects and NULL
  size_t argc = node->inputs.len + 4;
  char** argv = tmk_xrealloc(NULL, argc * sizeof(*argv));

  size_t arg = 0;
  argv[arg++] = config->archiver.len > 0 ? config->archiver.str : "ar";
  argv[arg++] = "rcs";
  argv[arg++] = node->output.str;
  for (size_t i = 0; i < node->inputs.len; ++i)
  {
    argv[arg++] = graph->items[node->inputs.items[i]].output.str;
  }
  argv[arg] = NULL;

  return argv;
}

// The command line as shown to the user and recorded in the command log
static tmk_str join_argv(char* const* argv)
{
  tmk_str line = tmk_str_create();
  for (size_t i = 0; argv[i] != NULL; ++i)
  {
    tmk_str_append_format(&line, i == 0 ? "%s" : " %s", argv[i]);
  }

  return line;
}

// Gives the node its command, once all of its inputs are known
static void set_command(tmk_node* node, char** argv)
{
  node->command_argv = argv;
  node->command_display = join_argv(argv);
}

// A source exists already: it has no command and starts as done
static size_t find_or_add_source(tmk_graph* graph, const tmk_str* path)
{
  size_t idx = find_node(graph, path);
  if (idx < graph->len)
  {
    return idx;
  }

  tmk_node source = {.status = TMK_NODE_DONE};
  source.output = tmk_str_clone(path);
  return add_node(graph, source);
}

// The object and depfile mirror the source path under TMK_OBJ_DIR, so that
// sources with the same name in different directories do not collide
static size_t find_or_add_object(tmk_graph* graph, const tmk_config* config,
                                 const tmk_str* source_path)
{
  tmk_str stem = tmk_path_without_ext(source_path);
  tmk_str object_path = tmk_str_from_cstr(TMK_OBJ_DIR);
  tmk_str_append_format(&object_path, "/%s.o", tmk_str_cstr(&stem));

  size_t idx = find_node(graph, &object_path);
  if (idx < graph->len)
  {
    tmk_str_free(&object_path);
    tmk_str_free(&stem);
    return idx;
  }

  tmk_node object = {.output = object_path};
  object.depfile_path = tmk_str_from_cstr(TMK_OBJ_DIR);
  tmk_str_append_format(&object.depfile_path, "/%s.d", tmk_str_cstr(&stem));
  tmk_str_free(&stem);

  size_t source_idx = find_or_add_source(graph, source_path);
  idx = add_node(graph, object);
  add_edge(graph, source_idx, idx);

  tmk_node* node = &graph->items[idx];
  set_command(node, build_object_argv(graph, config, node));
  return idx;
}

// Returns the index of the target called name, or config->targets.len if none
static size_t find_target(const tmk_config* config, const tmk_str* name)
{
  for (size_t i = 0; i < config->targets.len; ++i)
  {
    if (tmk_str_equal(&config->targets.items[i].name, name))
    {
      return i;
    }
  }

  return config->targets.len;
}

// Logs "<build file>:<line of target>: <message>" and returns false
static bool target_error(const tmk_target* target, const char* fmt, ...)
    __attribute__((format(printf, 2, 3)));

static bool target_error(const tmk_target* target, const char* fmt, ...)
{
  tmk_str msg = tmk_str_create();
  tmk_str_format(&msg, "%s:%zu: ", TMK_BUILD_FILE, target->line_no);

  va_list args;
  va_start(args, fmt);
  tmk_str_vappend_format(&msg, fmt, args);
  va_end(args);

  tmk_log_error("%s", tmk_str_cstr(&msg));
  tmk_str_free(&msg);

  return false;
}

// Every dependency must name a library
static bool check_deps_exist(const tmk_config* config)
{
  for (size_t i = 0; i < config->targets.len; ++i)
  {
    const tmk_target* target = &config->targets.items[i];
    for (size_t j = 0; j < target->deps.len; ++j)
    {
      const tmk_str* dep = &target->deps.items[j];
      size_t dep_idx = find_target(config, dep);
      if (dep_idx == config->targets.len)
      {
        return target_error(target, "'%s' depends on unknown library '%s'",
                            tmk_str_cstr(&target->name), tmk_str_cstr(dep));
      }
      if (config->targets.items[dep_idx].type != TMK_TARGET_LIBRARY)
      {
        return target_error(target,
                            "'%s' depends on '%s', which is not a library",
                            tmk_str_cstr(&target->name), tmk_str_cstr(dep));
      }
    }
  }

  return true;
}

typedef enum
{
  UNVISITED,
  VISITING,
  VISITED
} visit_state;

// Depth-first search from the target at idx. path holds the targets from the
// start of the search to idx, to show the cycle when there is one
static bool check_cycles_from(const tmk_config* config, size_t idx,
                              visit_state* states, tmk_index_array* path)
{
  states[idx] = VISITING;
  tmk_array_push(path, idx);

  const tmk_target* target = &config->targets.items[idx];
  for (size_t i = 0; i < target->deps.len; ++i)
  {
    size_t dep_idx = find_target(config, &target->deps.items[i]);
    if (states[dep_idx] == VISITING)
    {
      // The cycle starts where dep_idx is on the path
      size_t start = 0;
      while (path->items[start] != dep_idx)
      {
        ++start;
      }

      tmk_str cycle = tmk_str_create();
      for (size_t j = start; j < path->len; ++j)
      {
        const tmk_str* name = &config->targets.items[path->items[j]].name;
        tmk_str_append_format(&cycle, "%s -> ", tmk_str_cstr(name));
      }
      tmk_str_append_format(&cycle, "%s", tmk_str_cstr(&target->deps.items[i]));
      target_error(target, "dependency cycle: %s", tmk_str_cstr(&cycle));
      tmk_str_free(&cycle);
      return false;
    }
    if (states[dep_idx] == UNVISITED &&
        !check_cycles_from(config, dep_idx, states, path))
    {
      return false;
    }
  }

  path->len--;
  states[idx] = VISITED;
  return true;
}

static bool check_no_cycles(const tmk_config* config)
{
  visit_state* states = tmk_xrealloc(
      NULL, (config->targets.len + 1) * sizeof(*states));
  for (size_t i = 0; i < config->targets.len; ++i)
  {
    states[i] = UNVISITED;
  }
  tmk_index_array path = {0};

  bool ok = true;
  for (size_t i = 0; ok && i < config->targets.len; ++i)
  {
    if (states[i] == UNVISITED)
    {
      ok = check_cycles_from(config, i, states, &path);
    }
  }

  tmk_array_free(&path);
  free(states);
  return ok;
}

static size_t find_or_add_library(tmk_graph* graph, const tmk_config* config,
                                  size_t target_idx)
{
  const tmk_target* target = &config->targets.items[target_idx];
  tmk_str library_path = tmk_str_from_cstr(TMK_LIB_DIR);
  tmk_str_append_format(&library_path, "/lib%s.a", tmk_str_cstr(&target->name));

  size_t idx = find_node(graph, &library_path);
  if (idx < graph->len)
  {
    tmk_str_free(&library_path);
    return idx;
  }

  tmk_node library = {.output = library_path};
  idx = add_node(graph, library);

  for (size_t i = 0; i < target->sources.len; ++i)
  {
    size_t object_idx = find_or_add_object(graph, config,
                                           &target->sources.items[i]);
    add_edge(graph, object_idx, idx);
  }

  tmk_node* node = &graph->items[idx];
  set_command(node, build_archive_argv(graph, config, node));
  return idx;
}

// Adds to order the libraries that the target at idx depends on, directly or
// not, each after all the ones that depend on it within order
static void collect_libraries(const tmk_config* config, size_t idx,
                              bool* visited, tmk_index_array* order)
{
  const tmk_target* target = &config->targets.items[idx];
  for (size_t i = 0; i < target->deps.len; ++i)
  {
    size_t dep_idx = find_target(config, &target->deps.items[i]);
    if (!visited[dep_idx])
    {
      visited[dep_idx] = true;
      collect_libraries(config, dep_idx, visited, order);
      tmk_array_push(order, dep_idx);
    }
  }
}

// A binary links its objects, then the libraries it needs. A static library is
// only searched for what is undefined so far, so each library comes before the
// ones it depends on
static void add_binary(tmk_graph* graph, const tmk_config* config,
                       size_t target_idx)
{
  const tmk_target* target = &config->targets.items[target_idx];
  tmk_node binary = {0};
  binary.output = tmk_str_from_cstr(TMK_BIN_DIR);
  tmk_str_append_format(&binary.output, "/%s", tmk_str_cstr(&target->name));
  size_t idx = add_node(graph, binary);

  for (size_t i = 0; i < target->sources.len; ++i)
  {
    size_t object_idx = find_or_add_object(graph, config,
                                           &target->sources.items[i]);
    add_edge(graph, object_idx, idx);
  }

  // Dependencies come before what depends on them in order, so the link
  // order is order reversed
  bool* visited = tmk_xrealloc(NULL,
                               (config->targets.len + 1) * sizeof(*visited));
  for (size_t i = 0; i < config->targets.len; ++i)
  {
    visited[i] = false;
  }
  tmk_index_array order = {0};
  collect_libraries(config, target_idx, visited, &order);
  for (size_t i = order.len; i > 0; --i)
  {
    add_edge(graph, find_or_add_library(graph, config, order.items[i - 1]),
             idx);
  }
  tmk_array_free(&order);
  free(visited);

  tmk_node* node = &graph->items[idx];
  set_command(node, build_binary_argv(graph, config, node));
}

bool tmk_graph_build(tmk_graph* graph, const tmk_config* config)
{
  if (!check_deps_exist(config) || !check_no_cycles(config))
  {
    return false;
  }

  for (size_t i = 0; i < config->targets.len; ++i)
  {
    if (config->targets.items[i].type == TMK_TARGET_LIBRARY)
    {
      find_or_add_library(graph, config, i);
    }
    else
    {
      add_binary(graph, config, i);
    }
  }

  for (size_t i = 0; i < graph->len; ++i)
  {
    tmk_node* node = &graph->items[i];
    if (node->command_argv != NULL)
    {
      node->status = node->n_pending_inputs == 0 ? TMK_NODE_READY
                                                 : TMK_NODE_WAITING;
    }
  }

  return true;
}

static void free_node(tmk_node* node)
{
  tmk_str_free(&node->output);
  tmk_array_free(&node->inputs);
  tmk_array_free(&node->dependents);
  free(node->command_argv);
  tmk_str_free(&node->command_display);
  tmk_str_free(&node->depfile_path);
}

void tmk_graph_free(tmk_graph* graph)
{
  tmk_array_free_with(graph, free_node);
}
