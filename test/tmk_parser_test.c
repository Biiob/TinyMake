#include "tmk_parser.h"

#include <assert.h>
#include <unistd.h>

#include "test_util.h"
#include "tmk_parser_test.h"

/* Parses content into ctx and returns the result of parse_build_file. */
static int parse(const char* content, build_context* ctx)
{
  tmk_str path = test_write_temp_file(content);
  *ctx = (build_context){0};
  int rc = parse_build_file(ctx, &path);
  unlink(tmk_str_cstr(&path));
  tmk_str_free(&path);

  return rc;
}

static void check_invalid(const char* content)
{
  build_context ctx;
  assert(parse(content, &ctx) == -1);
  free_build_context(&ctx);
}

void test_parser_valid(void)
{
  build_context ctx;
  int rc = parse(
      "cc = gcc\n"
      "cflags = -Wall -O2\n"
      "ldflags = -lm\n"
      "includes = include test\n"
      "\n"
      "binary app: src/main.c src/util.c\n"
      "binary app_test: test/test.c\n",
      &ctx);
  assert(rc == 0);

  assert(tmk_str_equal_cstr(&ctx.compiler, "gcc"));
  assert(ctx.compile_flags.len == 2);
  assert(tmk_str_equal_cstr(&ctx.compile_flags.items[0], "-Wall"));
  assert(tmk_str_equal_cstr(&ctx.compile_flags.items[1], "-O2"));
  assert(ctx.link_flags.len == 1);
  assert(tmk_str_equal_cstr(&ctx.link_flags.items[0], "-lm"));
  assert(ctx.includes_dir.len == 2);
  assert(tmk_str_equal_cstr(&ctx.includes_dir.items[0], "include"));
  assert(tmk_str_equal_cstr(&ctx.includes_dir.items[1], "test"));

  assert(ctx.targets_count == 2);
  target* app = &ctx.targets[0];
  assert(app->type == BINARY);
  assert(tmk_str_equal_cstr(&app->name, "bin/app"));
  assert(app->source_files_count == 2);
  assert(tmk_str_equal_cstr(&app->source_files[0].c_path, "src/main.c"));
  assert(tmk_str_equal_cstr(&app->source_files[0].o_path, "obj/src/main.o"));
  assert(tmk_str_equal_cstr(&app->source_files[0].d_path, "obj/src/main.d"));
  assert(tmk_str_equal_cstr(&app->source_files[1].c_path, "src/util.c"));
  assert(tmk_str_equal_cstr(&app->source_files[1].o_path, "obj/src/util.o"));
  assert(tmk_str_equal_cstr(&app->source_files[1].d_path, "obj/src/util.d"));
  assert(tmk_str_equal_cstr(&ctx.targets[1].name, "bin/app_test"));
  assert(ctx.targets[1].source_files_count == 1);

  free_build_context(&ctx);
}

void test_parser_layout(void)
{
  /* Comments, blank lines, indentation and extra spaces are accepted. */
  build_context ctx;
  int rc = parse(
      "# A comment\n"
      "\n"
      "   \n"
      "  cc   =   clang  \n"
      "\t# Indented comment\n"
      "  binary   app:   src/main.c  \n",
      &ctx);
  assert(rc == 0);
  assert(tmk_str_equal_cstr(&ctx.compiler, "clang"));
  assert(ctx.targets_count == 1);
  assert(tmk_str_equal_cstr(&ctx.targets[0].name, "bin/app"));

  free_build_context(&ctx);
}

void test_parser_empty_list(void)
{
  /* An empty list is valid, and still counts as defined. */
  build_context ctx;
  assert(parse("cc = gcc\ncflags =\n", &ctx) == 0);
  assert(ctx.compile_flags.len == 0);
  free_build_context(&ctx);

  check_invalid("cc = gcc\ncflags =\ncflags = -O2\n");
}

/* Checks the object and depfile paths of the n-th source of target t. */
static void check_outputs(const target* t, size_t n, const char* o_path,
                          const char* d_path)
{
  assert(tmk_str_equal_cstr(&t->source_files[n].o_path, o_path));
  assert(tmk_str_equal_cstr(&t->source_files[n].d_path, d_path));
}

void test_parser_output_paths(void)
{
  /* Outputs mirror the source path under obj/, so that two sources with the
   * same name in different directories do not overwrite each other.
   */
  build_context ctx;
  int rc = parse(
      "cc = gcc\n"
      "binary app: main.c src/util.c src/b/util.c src/lib.v2/x.c src/noext\n"
      "binary app2: src/util.c\n",
      &ctx);
  assert(rc == 0);

  target* app = &ctx.targets[0];
  assert(app->source_files_count == 5);
  check_outputs(app, 0, "obj/main.o", "obj/main.d");
  check_outputs(app, 1, "obj/src/util.o", "obj/src/util.d");
  check_outputs(app, 2, "obj/src/b/util.o", "obj/src/b/util.d");
  check_outputs(app, 3, "obj/src/lib.v2/x.o", "obj/src/lib.v2/x.d");
  check_outputs(app, 4, "obj/src/noext.o", "obj/src/noext.d");

  /* A source shared by two binaries has the same outputs in both. */
  check_outputs(&ctx.targets[1], 0, "obj/src/util.o", "obj/src/util.d");

  free_build_context(&ctx);
}

void test_parser_invalid(void)
{
  /* Values */
  check_invalid("cc\n");
  check_invalid("cc =\n");
  check_invalid("cc = ccache gcc\n");
  check_invalid("cc gcc\n");
  check_invalid("cflags\n");
  check_invalid("cflags -Wall\n");

  /* Duplicates */
  check_invalid("cc = gcc\ncc = clang\n");
  check_invalid("cc = gcc\nldflags = -lm\nldflags = -lz\n");
  check_invalid("cc = gcc\nincludes = a\nincludes = b\n");
  check_invalid("cc = gcc\nbinary app: a.c\nbinary app: b.c\n");

  /* Binaries */
  check_invalid("cc = gcc\nbinary\n");
  check_invalid("cc = gcc\nbinary app\n");
  check_invalid("cc = gcc\nbinary app src/main.c\n");
  check_invalid("cc = gcc\nbinary :\n");
  check_invalid("cc = gcc\nbinary app:\n");

  /* Too many targets: the 9th one is rejected */
  check_invalid(
      "cc = gcc\n"
      "binary a1: a.c\nbinary a2: a.c\nbinary a3: a.c\n"
      "binary a4: a.c\nbinary a5: a.c\nbinary a6: a.c\n"
      "binary a7: a.c\nbinary a8: a.c\nbinary a9: a.c\n");

  /* Unknown key, including a typo */
  check_invalid("cc = gcc\ncflag = -Wall\n");

  /* No compiler */
  check_invalid("cflags = -Wall\nbinary app: src/main.c\n");
  check_invalid("");
}

void test_parser_missing_file(void)
{
  tmk_str path = tmk_str_from_cstr("/tmp/tmk_parser_test_does_not_exist");
  build_context ctx = {0};
  assert(parse_build_file(&ctx, &path) == -1);
  free_build_context(&ctx);
  tmk_str_free(&path);
}
