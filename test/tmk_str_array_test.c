#include "tmk_str_array.h"

#include <assert.h>
#include <stddef.h>

#include "tmk_str_array_test.h"

void test_str_array_zeroed(void)
{
  /* A zeroed array is empty and can be freed, even twice. */
  tmk_str_array arr = {0};
  assert(arr.len == 0);
  tmk_str_array_free(&arr);
  tmk_str_array_free(&arr);
  assert(arr.items == NULL);
}

void test_str_array_push(void)
{
  tmk_str_array arr = {0};
  tmk_str s = tmk_str_from_cstr("hello");
  char* buf = s.str;

  tmk_str_array_push(&arr, s);
  assert(arr.len == 1);
  assert(arr.capacity >= 1);
  /* Moved, not copied. */
  assert(arr.items[0].str == buf);
  assert(tmk_str_equal_cstr(&arr.items[0], "hello"));

  /* Freeing the array frees the strings it owns */
  tmk_str_array_free(&arr);
  assert(arr.items == NULL);
  assert(arr.len == 0);
  assert(arr.capacity == 0);
}

void test_str_array_push_grows(void)
{
  tmk_str_array arr = {0};
  tmk_str expected = tmk_str_create();

  for (int i = 0; i < 100; ++i)
  {
    tmk_str_format(&expected, "item %d", i);
    tmk_str_array_push(&arr, tmk_str_clone(&expected));
  }

  assert(arr.len == 100);
  assert(arr.capacity >= arr.len);
  for (int i = 0; i < 100; ++i)
  {
    tmk_str_format(&expected, "item %d", i);
    assert(tmk_str_equal(&arr.items[i], &expected));
  }

  tmk_str_free(&expected);
  tmk_str_array_free(&arr);
}

static void check_tokenize(const char* in, const char** expected,
                           size_t n_expected)
{
  tmk_str s = tmk_str_from_cstr(in);
  tmk_str_array toks = tmk_str_tokenize(&s, ' ');

  assert(toks.len == n_expected);
  if (n_expected == 0)
  {
    assert(toks.items == NULL);
  }
  for (size_t i = 0; i < n_expected; ++i)
  {
    assert(tmk_str_equal_cstr(&toks.items[i], expected[i]));
  }

  tmk_str_array_free(&toks);
  tmk_str_free(&s);
}

void test_tokenize(void)
{
  const char* abc[] = {"a", "b", "c"};
  check_tokenize("a b c", abc, 3);

  /* Leading, trailing and repeated delimiters produce no empty tokens. */
  const char* spaces[] = {"trailing", "spaces"};
  check_tokenize("  trailing  spaces  ", spaces, 2);

  const char* single[] = {"single"};
  check_tokenize("single", single, 1);

  check_tokenize("", NULL, 0);
  check_tokenize("   ", NULL, 0);
}
