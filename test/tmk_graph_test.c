#include "tmk_graph.h"

#include <assert.h>
#include <stdarg.h>
#include <stddef.h>

#include "tmk_graph_test.h"

// Adds a binary to config, with the sources given as a NULL-terminated list
static void add_binary(tmk_config* config, const char* name, ...)
{
  tmk_target target = {.type = TMK_TARGET_BINARY};
  target.name = tmk_str_from_cstr(name);

  va_list args;
  va_start(args, name);
  for (const char* source = va_arg(args, const char*); source != NULL;
       source = va_arg(args, const char*))
  {
    tmk_array_push(&target.sources, tmk_str_from_cstr(source));
  }
  va_end(args);

  tmk_array_push(&config->targets, target);
}

static tmk_config create_config(void)
{
  tmk_config config = {0};
  config.compiler = tmk_str_from_cstr("cc");
  return config;
}

// Returns the index of the node that produces output, which must exist
static size_t find(const tmk_graph* graph, const char* output)
{
  for (size_t i = 0; i < graph->len; ++i)
  {
    if (tmk_str_equal_cstr(&graph->items[i].output, output))
    {
      return i;
    }
  }

  assert(!"node not found");
  return graph->len;
}

static size_t count(const tmk_graph* graph, const char* output)
{
  size_t n = 0;
  for (size_t i = 0; i < graph->len; ++i)
  {
    n += tmk_str_equal_cstr(&graph->items[i].output, output);
  }

  return n;
}

static bool contains(const tmk_index_array* indices, size_t idx)
{
  for (size_t i = 0; i < indices->len; ++i)
  {
    if (indices->items[i] == idx)
    {
      return true;
    }
  }

  return false;
}

// Checks that the node at idx has exactly the given inputs, in order
static void check_inputs(const tmk_graph* graph, size_t idx, size_t n, ...)
{
  const tmk_index_array* inputs = &graph->items[idx].inputs;
  assert(inputs->len == n);

  va_list args;
  va_start(args, n);
  for (size_t i = 0; i < n; ++i)
  {
    assert(inputs->items[i] == find(graph, va_arg(args, const char*)));
  }
  va_end(args);
}

void test_graph_output_paths(void)
{
  // Outputs mirror the source path under obj/, so that two sources with the
  // same name in different directories do not overwrite each other
  tmk_config config = create_config();
  add_binary(&config, "app", "main.c", "src/util.c", "src/b/util.c",
             "src/lib.v2/x.c", "src/noext", NULL);
  tmk_graph graph = {0};
  assert(tmk_graph_build(&graph, &config));

  const char* expected[][2] = {
      {"obj/main.o", "obj/main.d"},
      {"obj/src/util.o", "obj/src/util.d"},
      {"obj/src/b/util.o", "obj/src/b/util.d"},
      {"obj/src/lib.v2/x.o", "obj/src/lib.v2/x.d"},
      {"obj/src/noext.o", "obj/src/noext.d"},
  };
  for (size_t i = 0; i < sizeof(expected) / sizeof(expected[0]); ++i)
  {
    const tmk_node* object = &graph.items[find(&graph, expected[i][0])];
    assert(tmk_str_equal_cstr(&object->depfile_path, expected[i][1]));
  }
  find(&graph, "bin/app");

  tmk_graph_free(&graph);
  tmk_config_free(&config);
}

void test_graph_shared_source(void)
{
  // A source used by two binaries is one source node and one object node,
  // compiled once and linked into both
  tmk_config config = create_config();
  add_binary(&config, "app", "src/main.c", "src/util.c", NULL);
  add_binary(&config, "app2", "src/util.c", NULL);
  tmk_graph graph = {0};
  assert(tmk_graph_build(&graph, &config));

  assert(graph.len == 6);
  assert(count(&graph, "src/util.c") == 1);
  assert(count(&graph, "obj/src/util.o") == 1);

  const tmk_node* util_o = &graph.items[find(&graph, "obj/src/util.o")];
  assert(util_o->dependents.len == 2);
  assert(contains(&util_o->dependents, find(&graph, "bin/app")));
  assert(contains(&util_o->dependents, find(&graph, "bin/app2")));

  tmk_graph_free(&graph);
  tmk_config_free(&config);
}

void test_graph_edges(void)
{
  tmk_config config = create_config();
  add_binary(&config, "app", "src/main.c", "src/util.c", NULL);
  add_binary(&config, "app2", "src/util.c", "src/other.c", NULL);
  tmk_graph graph = {0};
  assert(tmk_graph_build(&graph, &config));

  // Binaries take objects, never sources, in the order of the build file
  check_inputs(&graph, find(&graph, "bin/app"), 2, "obj/src/main.o",
               "obj/src/util.o");
  check_inputs(&graph, find(&graph, "bin/app2"), 2, "obj/src/util.o",
               "obj/src/other.o");
  check_inputs(&graph, find(&graph, "obj/src/util.o"), 1, "src/util.c");
  check_inputs(&graph, find(&graph, "src/util.c"), 0);

  // inputs and dependents describe the same edges, from both ends
  size_t n_inputs = 0;
  size_t n_dependents = 0;
  for (size_t i = 0; i < graph.len; ++i)
  {
    const tmk_node* node = &graph.items[i];
    for (size_t j = 0; j < node->inputs.len; ++j)
    {
      assert(contains(&graph.items[node->inputs.items[j]].dependents, i));
    }
    for (size_t j = 0; j < node->dependents.len; ++j)
    {
      assert(contains(&graph.items[node->dependents.items[j]].inputs, i));
    }
    n_inputs += node->inputs.len;
    n_dependents += node->dependents.len;
  }
  assert(n_inputs == n_dependents);

  tmk_graph_free(&graph);
  tmk_config_free(&config);
}

void test_graph_initial_status(void)
{
  // Sources exist already, objects can start, binaries wait for their objects
  tmk_config config = create_config();
  add_binary(&config, "app", "src/main.c", "src/util.c", NULL);
  add_binary(&config, "app2", "src/util.c", NULL);
  tmk_graph graph = {0};
  assert(tmk_graph_build(&graph, &config));

  const tmk_node* source = &graph.items[find(&graph, "src/util.c")];
  assert(source->status == TMK_NODE_DONE);
  assert(source->command_argv == NULL);

  const tmk_node* object = &graph.items[find(&graph, "obj/src/util.o")];
  assert(object->status == TMK_NODE_READY);
  assert(object->n_pending_inputs == 0);

  const tmk_node* app = &graph.items[find(&graph, "bin/app")];
  assert(app->status == TMK_NODE_WAITING);
  assert(app->n_pending_inputs == 2);

  const tmk_node* app2 = &graph.items[find(&graph, "bin/app2")];
  assert(app2->status == TMK_NODE_WAITING);
  assert(app2->n_pending_inputs == 1);

  tmk_graph_free(&graph);
  tmk_config_free(&config);
}

void test_graph_commands(void)
{
  // The exact command lines matter: the command log compares them, so any
  // change rebuilds everything
  tmk_config config = create_config();
  tmk_array_push(&config.compile_flags, tmk_str_from_cstr("-O2"));
  tmk_array_push(&config.link_flags, tmk_str_from_cstr("-lm"));
  tmk_array_push(&config.include_dirs, tmk_str_from_cstr("inc"));
  add_binary(&config, "app", "src/main.c", "src/util.c", NULL);
  tmk_graph graph = {0};
  assert(tmk_graph_build(&graph, &config));

  const tmk_node* object = &graph.items[find(&graph, "obj/src/main.o")];
  assert(tmk_str_equal_cstr(&object->command_display,
                            "cc -O2 -I inc -fdiagnostics-color=always -MMD "
                            "-MF obj/src/main.d -c src/main.c "
                            "-o obj/src/main.o"));

  const tmk_node* app = &graph.items[find(&graph, "bin/app")];
  assert(tmk_str_equal_cstr(&app->command_display,
                            "cc -lm -o bin/app obj/src/main.o obj/src/util.o"));

  tmk_graph_free(&graph);
  tmk_config_free(&config);
}
