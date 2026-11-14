#include "tmk_parser.h"

#include <assert.h>
#include <unistd.h>

#include "test_util.h"
#include "tmk_parser_test.h"

// Parses content into config and returns the result of tmk_parser_parse
static bool parse(const char* content, tmk_config* config)
{
  tmk_str path = test_write_temp_file(content);
  *config = (tmk_config){0};
  bool ok = tmk_parser_parse(config, &path);
  unlink(tmk_str_cstr(&path));
  tmk_str_free(&path);

  return ok;
}

static void check_invalid(const char* content)
{
  tmk_config config;
  assert(!parse(content, &config));
  tmk_config_free(&config);
}

void test_parser_valid(void)
{
  tmk_config config;
  bool ok = parse(
      "cc = gcc\n"
      "cflags = -Wall -O2\n"
      "ldflags = -lm\n"
      "includes = include test\n"
      "\n"
      "binary app: src/main.c src/util.c\n"
      "binary app_test: test/test.c\n",
      &config);
  assert(ok);

  assert(tmk_str_equal_cstr(&config.compiler, "gcc"));
  assert(config.compile_flags.len == 2);
  assert(tmk_str_equal_cstr(&config.compile_flags.items[0], "-Wall"));
  assert(tmk_str_equal_cstr(&config.compile_flags.items[1], "-O2"));
  assert(config.link_flags.len == 1);
  assert(tmk_str_equal_cstr(&config.link_flags.items[0], "-lm"));
  assert(config.include_dirs.len == 2);
  assert(tmk_str_equal_cstr(&config.include_dirs.items[0], "include"));
  assert(tmk_str_equal_cstr(&config.include_dirs.items[1], "test"));

  assert(config.targets.len == 2);
  tmk_target* app = &config.targets.items[0];
  assert(app->type == TMK_TARGET_BINARY);
  assert(tmk_str_equal_cstr(&app->name, "app"));
  assert(app->line_no == 6);
  assert(app->sources.len == 2);
  assert(tmk_str_equal_cstr(&app->sources.items[0], "src/main.c"));
  assert(tmk_str_equal_cstr(&app->sources.items[1], "src/util.c"));
  assert(app->deps.len == 0);

  tmk_target* app_test = &config.targets.items[1];
  assert(tmk_str_equal_cstr(&app_test->name, "app_test"));
  assert(app_test->line_no == 7);
  assert(app_test->sources.len == 1);
  assert(tmk_str_equal_cstr(&app_test->sources.items[0], "test/test.c"));

  tmk_config_free(&config);
}

void test_parser_layout(void)
{
  // Comments, blank lines, indentation and extra spaces are accepted
  tmk_config config;
  bool ok = parse(
      "# A comment\n"
      "\n"
      "   \n"
      "  cc   =   clang  \n"
      "\t# Indented comment\n"
      "  binary   app:   src/main.c  \n",
      &config);
  assert(ok);
  assert(tmk_str_equal_cstr(&config.compiler, "clang"));
  assert(config.targets.len == 1);
  assert(tmk_str_equal_cstr(&config.targets.items[0].name, "app"));
  assert(config.targets.items[0].line_no == 6);
  assert(config.targets.items[0].sources.len == 1);

  tmk_config_free(&config);
}

void test_parser_empty_list(void)
{
  // An empty list is valid, and still counts as defined
  tmk_config config;
  assert(parse("cc = gcc\ncflags =\n", &config));
  assert(config.compile_flags.len == 0);
  tmk_config_free(&config);

  check_invalid("cc = gcc\ncflags =\ncflags = -O2\n");
}

void test_parser_many_targets(void)
{
  // Targets are kept in a growable array: there is no maximum
  tmk_str content = tmk_str_from_cstr("cc = gcc\n");
  for (int i = 1; i <= 20; ++i)
  {
    tmk_str_append_format(&content, "binary a%d: a.c\n", i);
  }

  tmk_config config;
  assert(parse(tmk_str_cstr(&content), &config));
  assert(config.targets.len == 20);
  assert(tmk_str_equal_cstr(&config.targets.items[19].name, "a20"));
  assert(config.targets.items[19].line_no == 21);

  tmk_config_free(&config);
  tmk_str_free(&content);
}

void test_parser_invalid(void)
{
  // Values
  check_invalid("cc\n");
  check_invalid("cc =\n");
  check_invalid("cc = ccache gcc\n");
  check_invalid("cc gcc\n");
  check_invalid("cflags\n");
  check_invalid("cflags -Wall\n");

  // Duplicates
  check_invalid("cc = gcc\ncc = clang\n");
  check_invalid("cc = gcc\nldflags = -lm\nldflags = -lz\n");
  check_invalid("cc = gcc\nincludes = a\nincludes = b\n");
  check_invalid("cc = gcc\nbinary app: a.c\nbinary app: b.c\n");

  // Binaries
  check_invalid("cc = gcc\nbinary\n");
  check_invalid("cc = gcc\nbinary app\n");
  check_invalid("cc = gcc\nbinary app src/main.c\n");
  check_invalid("cc = gcc\nbinary :\n");
  check_invalid("cc = gcc\nbinary app:\n");

  // Unknown key, including a typo
  check_invalid("cc = gcc\ncflag = -Wall\n");

  // No compiler
  check_invalid("cflags = -Wall\nbinary app: src/main.c\n");
  check_invalid("");
}

void test_parser_missing_file(void)
{
  tmk_str path = tmk_str_from_cstr("/tmp/tmk_parser_test_does_not_exist");
  tmk_config config = {0};
  assert(!tmk_parser_parse(&config, &path));
  tmk_config_free(&config);
  tmk_str_free(&path);
}
