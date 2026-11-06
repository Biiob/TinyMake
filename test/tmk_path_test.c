#include "tmk_path_test.h"

#include <assert.h>

#include "tmk_path.h"
#include "tmk_str.h"

void test_path_get_dir(void)
{
  tmk_str s = tmk_str_from_cstr("/test/absolute/path.txt");
  tmk_str dir = tmk_path_get_dir(&s);
  assert(tmk_str_equal_cstr(&dir, "/test/absolute"));
  tmk_str_free(&dir);

  tmk_str_format(&s, "%s", "test/relative/path.txt");
  dir = tmk_path_get_dir(&s);
  assert(tmk_str_equal_cstr(&dir, "test/relative"));
  tmk_str_free(&dir);

  tmk_str_format(&s, "%s", "test_no_dir.txt");
  dir = tmk_path_get_dir(&s);
  assert(tmk_str_equal_cstr(&dir, ""));
  tmk_str_free(&dir);

  tmk_str_free(&s);
}
void test_path_without_ext(void)
{
  tmk_str s = tmk_str_from_cstr("/test/with/dir.txt");
  tmk_str no_ext = tmk_path_without_ext(&s);
  assert(tmk_str_equal_cstr(&no_ext, "/test/with/dir"));
  tmk_str_free(&no_ext);

  tmk_str_format(&s, "%s", "no_ext");
  no_ext = tmk_path_without_ext(&s);
  assert(tmk_str_equal_cstr(&no_ext, "no_ext"));
  tmk_str_free(&no_ext);

  tmk_str_format(&s, "%s", "test_no_dir.txt");
  no_ext = tmk_path_without_ext(&s);
  assert(tmk_str_equal_cstr(&no_ext, "test_no_dir"));
  tmk_str_free(&no_ext);

  tmk_str_format(&s, "%s", "test/dot.in/dir");
  no_ext = tmk_path_without_ext(&s);
  assert(tmk_str_equal_cstr(&no_ext, "test/dot.in/dir"));
  tmk_str_free(&no_ext);

  tmk_str_free(&s);
}
