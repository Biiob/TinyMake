#include "tmk_depfile.h"

#include <assert.h>
#include <string.h>
#include <unistd.h>

#include "test_util.h"
#include "tmk_depfile_test.h"

// Parses content and checks the result against the expected paths
static void check_depfile(const char* content, const char** expected,
                          size_t n_expected)
{
  tmk_str path = test_write_temp_file(content);

  tmk_str_array paths;
  assert(tmk_depfile_parse(&paths, &path));
  assert(paths.len == n_expected);

  if (n_expected == 0)
  {
    assert(paths.items == NULL);
  }

  for (size_t i = 0; i < n_expected; ++i)
  {
    assert(tmk_str_equal_cstr(&paths.items[i], expected[i]));
  }

  tmk_str_array_free(&paths);
  unlink(tmk_str_cstr(&path));
  tmk_str_free(&path);
}

#define CHECK_DEPFILE(content, ...)                                       \
  do                                                                      \
  {                                                                       \
    const char* expected[] = {__VA_ARGS__};                               \
    check_depfile(content, expected, sizeof expected / sizeof *expected); \
  } while (0)

void test_depfile_single_line(void)
{
  CHECK_DEPFILE("obj/a.o: src/a.c include/a.h\n", "src/a.c", "include/a.h");
}

void test_depfile_continuation_lines(void)
{
  CHECK_DEPFILE(
      "obj/a.o: src/a.c include/a.h \\\n"
      " include/b.h \\\n"
      " include/c.h\n",
      "src/a.c", "include/a.h", "include/b.h", "include/c.h");
}

void test_depfile_no_trailing_newline(void)
{
  CHECK_DEPFILE("obj/a.o: src/a.c include/a.h", "src/a.c", "include/a.h");
}

void test_depfile_extra_whitespace(void)
{
  CHECK_DEPFILE(
      "obj/a.o:   src/a.c include/a.h  \\\n"
      "\t  include/b.h   \n",
      "src/a.c", "include/a.h", "include/b.h");
}

void test_depfile_no_discovered_inputs(void)
{
  check_depfile("obj/a.o:\n", NULL, 0);
}

void test_depfile_empty(void)
{
  check_depfile("", NULL, 0);
}

void test_depfile_missing_file(void)
{
  tmk_str path = tmk_str_from_cstr("/tmp/tmk_depfile_test_does_not_exist.d");

  // Garbage on entry: tmk_depfile_parse must still leave an empty array
  tmk_str_array paths;
  memset(&paths, 0xAB, sizeof paths);
  assert(!tmk_depfile_parse(&paths, &path));
  assert(paths.items == NULL);
  assert(paths.len == 0);

  tmk_str_free(&path);
}
