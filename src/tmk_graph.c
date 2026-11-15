#include "tmk_graph.h"

#include <stdlib.h>

#include "tmk_constants.h"
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

static void add_binary(tmk_graph* graph, const tmk_config* config,
                       const tmk_target* target)
{
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

  tmk_node* node = &graph->items[idx];
  set_command(node, build_binary_argv(graph, config, node));
}

bool tmk_graph_build(tmk_graph* graph, const tmk_config* config)
{
  for (size_t i = 0; i < config->targets.len; ++i)
  {
    add_binary(graph, config, &config->targets.items[i]);
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
