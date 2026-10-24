#include "tinymake_string.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "tinymake_string_test.h"

/* Invariants every valid string_t must satisfy. */
static void check_invariants(const string_t* s)
{
  assert(s->str != NULL);
  assert(s->len <= s->capacity);
  assert(strlen(s->str) == s->len);
}

/* Heap copy for string_take */
static char* heap_copy(const char* s)
{
  size_t n = strlen(s) + 1;
  char* p = malloc(n);
  assert(p != NULL);
  memcpy(p, s, n);
  return p;
}

void test_create(void)
{
  string_t s = string_create();
  check_invariants(&s);
  assert(s.len == 0);
  assert(s.capacity >= DEFAULT_STRING_SIZE);
  assert(s.str[0] == '\0');
  string_free(&s);
}

void test_create_with_capacity(void)
{
  string_t s = string_create_with_capacity(10);
  check_invariants(&s);
  assert(s.len == 0);
  assert(s.capacity >= 10);
  string_free(&s);

  /* Edge case: zero capacity must still yield a valid empty string. */
  string_t z = string_create_with_capacity(0);
  check_invariants(&z);
  assert(z.len == 0);
  string_free(&z);
}

void test_reserve(void)
{
  string_t s = string_from_cstr("hello");
  string_reserve(&s, 1024);
  check_invariants(&s);
  assert(s.capacity >= 1024);
  assert(strcmp(s.str, "hello") == 0); /* content preserved */

  /* Reserving less than current capacity must not shrink or corrupt. */
  size_t cap = s.capacity;
  string_reserve(&s, 1);
  check_invariants(&s);
  assert(s.capacity >= cap);
  assert(strcmp(s.str, "hello") == 0);
  string_free(&s);
}

void test_free(void)
{
  string_t s = string_from_cstr("abc");
  string_free(&s);
  assert(s.str == NULL);
  assert(s.len == 0);
  assert(s.capacity == 0);
  string_free(&s);
}

void test_copy_from_char_array(void)
{
  const char* src = "hello world";
  string_t s = string_from_cstr(src);
  check_invariants(&s);
  assert(s.len == strlen(src));
  assert(strcmp(s.str, src) == 0);
  assert(s.str != src); /* actually a copy */
  string_free(&s);

  string_t e = string_from_cstr("");
  check_invariants(&e);
  assert(e.len == 0);
  string_free(&e);
}

void test_copy_from_string(void)
{
  string_t a = string_from_cstr("original");
  string_t b = string_clone(&a);
  check_invariants(&b);
  assert(b.len == a.len);
  assert(strcmp(a.str, b.str) == 0);
  assert(a.str != b.str); /* deep copy */

  /* Mutating the copy must not affect the source. */
  b.str[0] = 'X';
  assert(a.str[0] == 'o');

  string_free(&a);
  string_free(&b);
}

void test_assign(void)
{
  char* raw = heap_copy("owned");
  string_t s = string_take(raw);
  check_invariants(&s);
  assert(s.str == raw); /* took ownership, no copy */
  assert(s.len == 5);
  string_free(&s);
}

void test_get(void)
{
  string_t s = string_from_cstr("data");
  char* p = string_cstr(&s);
  assert(p == s.str);
  assert(strcmp(p, "data") == 0);
  string_free(&s);
}

void test_at(void)
{
  string_t s = string_from_cstr("abc");

  char* first = string_at(&s, 0);
  assert(first == &s.str[0]);
  assert(*first == 'a');

  char* last = string_at(&s, s.len - 1);
  assert(last != NULL && *last == 'c');

  /* Returned pointer is writable and aliases the buffer. */
  *string_at(&s, 1) = 'X';
  assert(strcmp(s.str, "aXc") == 0);

  /* Bounds checking: the terminator and beyond are out of range. */
  assert(string_at(&s, s.len) == NULL);
  assert(string_at(&s, s.len + 100) == NULL);
  assert(string_at(&s, (size_t)-1) == NULL);
  string_free(&s);

  string_t e = string_create();
  assert(string_at(&e, 0) == NULL);
  string_free(&e);
}

void test_append_from_char_array(void)
{
  string_t s = string_from_cstr("foo");
  string_t* r = string_append_cstr(&s, "bar");
  assert(r == &s);
  check_invariants(&s);
  assert(strcmp(s.str, "foobar") == 0);
  assert(s.len == 6);

  /* Appending empty is a no-op. */
  string_append_cstr(&s, "");
  assert(strcmp(s.str, "foobar") == 0);
  string_free(&s);
}

void test_append_from_string(void)
{
  string_t dst = string_create();
  string_t src = string_from_cstr("abc");
  string_t* r = string_append(&dst, &src);
  assert(r == &dst);
  check_invariants(&dst);
  assert(strcmp(dst.str, "abc") == 0);
  assert(strcmp(src.str, "abc") == 0); /* source untouched */

  /* Self-append */
  string_append(&dst, &dst);
  check_invariants(&dst);
  assert(strcmp(dst.str, "abcabc") == 0);

  string_free(&src);
  string_free(&dst);
}

void test_append_grows(void)
{
  string_t s = string_create_with_capacity(4);
  for (int i = 0; i < 1000; ++i)
    string_append_cstr(&s, "x");
  check_invariants(&s);
  assert(s.len == 1000);
  for (size_t i = 0; i < s.len; ++i)
    assert(s.str[i] == 'x');
  string_free(&s);
}

void test_format(void)
{
  string_t s = string_create();
  string_t* r = string_format(&s, "%d-%s-%c", 42, "abc", 'z');
  assert(r == &s);
  check_invariants(&s);
  assert(strcmp(s.str, "42-abc-z") == 0);

  string_format(&s, "%s", "new");
  assert(strcmp(s.str, "new") == 0);
  string_free(&s);
}

void test_format_grows(void)
{
  /* Output longer than the initial capacity: must resize, not truncate. */
  char big[2000];
  memset(big, 'a', sizeof big - 1);
  big[sizeof big - 1] = '\0';

  string_t s = string_create_with_capacity(8);
  string_format(&s, "[%s]", big);
  check_invariants(&s);
  assert(s.len == sizeof big - 1 + 2);
  assert(s.str[0] == '[' && s.str[s.len - 1] == ']');
  string_free(&s);
}

static void check_trim(const char* in, const char* expected)
{
  string_t s = string_from_cstr(in);
  string_t* r = string_trim(&s);
  assert(r == &s);
  check_invariants(&s);
  assert(strcmp(s.str, expected) == 0);
  assert(s.len == strlen(expected));
  string_free(&s);
}

void test_trim(void)
{
  check_trim("  hello  ", "hello");
  check_trim("hello", "hello");
  check_trim("\t\n hello \r\n", "hello");
  check_trim("   leading", "leading");
  check_trim("trailing   ", "trailing");
  check_trim("  in  side  ", "in  side");
  check_trim("     ", "");
  check_trim("", "");
  check_trim(" x ", "x");
}
