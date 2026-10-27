#include "tmk_str.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "tmk_str_test.h"

/* Invariants every valid tmk_str must satisfy. */
static void check_invariants(const tmk_str* s)
{
  assert(s->str != NULL);
  assert(s->len <= s->capacity);
  assert(strlen(s->str) == s->len);
}

/* Heap copy for tmk_str_take */
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
  tmk_str s = tmk_str_create();
  check_invariants(&s);
  assert(s.len == 0);
  assert(s.capacity >= TMK_STR_DEFAULT_CAP);
  assert(s.str[0] == '\0');
  tmk_str_free(&s);
}

void test_create_with_capacity(void)
{
  tmk_str s = tmk_str_create_with_capacity(10);
  check_invariants(&s);
  assert(s.len == 0);
  assert(s.capacity >= 10);
  tmk_str_free(&s);

  /* Edge case: zero capacity must still yield a valid empty string. */
  tmk_str z = tmk_str_create_with_capacity(0);
  check_invariants(&z);
  assert(z.len == 0);
  tmk_str_free(&z);
}

void test_reserve(void)
{
  tmk_str s = tmk_str_from_cstr("hello");
  tmk_str_reserve(&s, 1024);
  check_invariants(&s);
  assert(s.capacity >= 1024);
  assert(strcmp(s.str, "hello") == 0); /* content preserved */

  /* Reserving less than current capacity must not shrink or corrupt. */
  size_t cap = s.capacity;
  tmk_str_reserve(&s, 1);
  check_invariants(&s);
  assert(s.capacity >= cap);
  assert(strcmp(s.str, "hello") == 0);
  tmk_str_free(&s);
}

void test_clear(void)
{
  tmk_str s = tmk_str_from_cstr("hello");
  tmk_str_clear(&s);
  check_invariants(&s);
  assert(s.len == 0);
  assert(strcmp(s.str, "") == 0);
  tmk_str_free(&s);
}

void test_free(void)
{
  tmk_str s = tmk_str_from_cstr("abc");
  tmk_str_free(&s);
  assert(s.str == NULL);
  assert(s.len == 0);
  assert(s.capacity == 0);
  tmk_str_free(&s);
}

void test_from_buf(void)
{
  const char* src = "hello world";
  tmk_str s = tmk_str_from_buf(src, 8);
  check_invariants(&s);
  assert(s.len == 8);
  assert(strcmp(s.str, "hello wo") == 0);
  tmk_str_free(&s);
}

void test_from_cstr(void)
{
  const char* src = "hello world";
  tmk_str s = tmk_str_from_cstr(src);
  check_invariants(&s);
  assert(s.len == strlen(src));
  assert(strcmp(s.str, src) == 0);
  assert(s.str != src); /* actually a copy */
  tmk_str_free(&s);

  tmk_str e = tmk_str_from_cstr("");
  check_invariants(&e);
  assert(e.len == 0);
  tmk_str_free(&e);
}

void test_clone(void)
{
  tmk_str a = tmk_str_from_cstr("original");
  tmk_str b = tmk_str_clone(&a);
  check_invariants(&b);
  assert(b.len == a.len);
  assert(strcmp(a.str, b.str) == 0);
  assert(a.str != b.str); /* deep copy */

  /* Mutating the copy must not affect the source. */
  b.str[0] = 'X';
  assert(a.str[0] == 'o');

  tmk_str_free(&a);
  tmk_str_free(&b);
}

void test_substr(void)
{
  tmk_str a = tmk_str_from_cstr("hello world");
  tmk_str b = tmk_str_substr(&a, 0, 5);
  check_invariants(&b);
  assert(b.len == 5);
  assert(strcmp(b.str, "hello") == 0);
  assert(a.str != b.str);

  tmk_str c = tmk_str_substr(&a, 0, TMK_STR_NPOS);
  check_invariants(&c);
  assert(strcmp(c.str, "hello world") == 0);
  assert(a.str != c.str);

  tmk_str d = tmk_str_substr(&a, 15, 5);
  check_invariants(&d);
  assert(strcmp(d.str, "") == 0);

  tmk_str e = tmk_str_substr(&a, 3, 0);
  check_invariants(&e);
  assert(e.len == 0);

  tmk_str f = tmk_str_substr(&a, 6, TMK_STR_NPOS);
  check_invariants(&f);
  assert(strcmp(f.str, "world") == 0);

  tmk_str_free(&a);
  tmk_str_free(&b);
  tmk_str_free(&c);
  tmk_str_free(&d);
  tmk_str_free(&e);
  tmk_str_free(&f);
}

void test_take(void)
{
  char* raw = heap_copy("owned");
  tmk_str s = tmk_str_take(raw);
  check_invariants(&s);
  assert(s.str == raw); /* took ownership, no copy */
  assert(s.len == 5);
  tmk_str_free(&s);
}

void test_release(void)
{
  tmk_str s = tmk_str_from_cstr("owned");
  char* raw = tmk_str_release(&s);
  assert(s.str == NULL);
  assert(s.len == 0);
  assert(s.capacity == 0);
  assert(strcmp(raw, "owned") == 0);
  tmk_str_free(&s);
  free(raw);
}

void test_cstr(void)
{
  tmk_str s = tmk_str_from_cstr("data");
  const char* p = tmk_str_cstr(&s);
  assert(p == s.str);
  assert(strcmp(p, "data") == 0);
  tmk_str_free(&s);
}

void test_set(void)
{
  tmk_str s = tmk_str_from_cstr("abc");

  tmk_str_set(&s, 1, 'X');
  assert(strcmp(s.str, "aXc") == 0);

  /* Bounds checking: trying to write beyond the length does nothing */
  tmk_str_set(&s, 4, 'Z');
  assert(strcmp(s.str, "aXc") == 0);

  tmk_str_free(&s);
}

void test_append_n(void)
{
  tmk_str s = tmk_str_from_cstr("foo");
  tmk_str* r = tmk_str_append_n(&s, "barxzy", 3);
  assert(r == &s);
  check_invariants(&s);
  assert(strcmp(s.str, "foobar") == 0);
  assert(s.len == 6);
  tmk_str_free(&s);
}

void test_append_cstr(void)
{
  tmk_str s = tmk_str_from_cstr("foo");
  tmk_str* r = tmk_str_append_cstr(&s, "bar");
  assert(r == &s);
  check_invariants(&s);
  assert(strcmp(s.str, "foobar") == 0);
  assert(s.len == 6);

  /* Appending empty is a no-op. */
  tmk_str_append_cstr(&s, "");
  assert(strcmp(s.str, "foobar") == 0);
  tmk_str_free(&s);
}

void test_append(void)
{
  tmk_str dst = tmk_str_create();
  tmk_str src = tmk_str_from_cstr("abc");
  tmk_str* r = tmk_str_append(&dst, &src);
  assert(r == &dst);
  check_invariants(&dst);
  assert(strcmp(dst.str, "abc") == 0);
  assert(strcmp(src.str, "abc") == 0); /* source untouched */

  /* Self-append */
  tmk_str_append(&dst, &dst);
  check_invariants(&dst);
  assert(strcmp(dst.str, "abcabc") == 0);

  tmk_str_free(&src);
  tmk_str_free(&dst);

  tmk_str long_str =
      tmk_str_from_cstr("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ");

  tmk_str_append(&long_str, &long_str);
  check_invariants(&long_str);
  assert(strcmp(long_str.str,
                "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghij"
                "klmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ") == 0);

  tmk_str_append_n(&long_str, long_str.str + 26, 26);
  check_invariants(&long_str);
  assert(long_str.len == 130);
  assert(memcmp(long_str.str + 104, "ABCDEFGHIJKLMNOPQRSTUVWXYZ", 26) == 0);

  tmk_str_free(&long_str);
}

void test_append_grows(void)
{
  tmk_str s = tmk_str_create_with_capacity(4);
  for (int i = 0; i < 1000; ++i)
    tmk_str_append_cstr(&s, "x");
  check_invariants(&s);
  assert(s.len == 1000);
  assert(s.capacity < 2 * (s.len + 1));
  for (size_t i = 0; i < s.len; ++i)
    assert(s.str[i] == 'x');
  tmk_str_free(&s);
}

void test_append_format(void)
{
  tmk_str s = tmk_str_from_cstr("hello");
  char u[] = "xxx";
  tmk_str_append_format(&s, " user %s!", u);
  check_invariants(&s);
  assert(strcmp(s.str, "hello user xxx!") == 0);

  tmk_str_free(&s);
}

void test_push(void)
{
  tmk_str s = tmk_str_from_cstr("foo");
  tmk_str_push(&s, 'b');
  check_invariants(&s);
  assert(strcmp(s.str, "foob") == 0);
  assert(s.len == 4);

  tmk_str_push(&s, '\0');
  check_invariants(&s);
  assert(strcmp(s.str, "foob") == 0);
  assert(s.len == 4);
  tmk_str_free(&s);
}

void test_format(void)
{
  tmk_str s = tmk_str_create();
  tmk_str* r = tmk_str_format(&s, "%d-%s-%c", 42, "abc", 'z');
  assert(r == &s);
  check_invariants(&s);
  assert(strcmp(s.str, "42-abc-z") == 0);

  tmk_str_format(&s, "%s", "new");
  assert(strcmp(s.str, "new") == 0);
  tmk_str_free(&s);
}

void test_format_grows(void)
{
  /* Output longer than the initial capacity: must resize, not truncate. */
  char big[2000];
  memset(big, 'a', sizeof big - 1);
  big[sizeof big - 1] = '\0';

  tmk_str s = tmk_str_create_with_capacity(8);
  tmk_str_format(&s, "[%s]", big);
  check_invariants(&s);
  assert(s.len == sizeof big - 1 + 2);
  assert(s.str[0] == '[' && s.str[s.len - 1] == ']');
  tmk_str_free(&s);
}

static void check_trim(const char* in, const char* expected)
{
  tmk_str s = tmk_str_from_cstr(in);
  tmk_str* r = tmk_str_trim(&s);
  assert(r == &s);
  check_invariants(&s);
  assert(strcmp(s.str, expected) == 0);
  assert(s.len == strlen(expected));
  tmk_str_free(&s);
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

void test_equal(void)
{
  tmk_str s1 = tmk_str_from_cstr("hello");
  tmk_str s2 = tmk_str_from_cstr("hallo");
  assert(!tmk_str_equal(&s1, &s2));

  tmk_str_format(&s2, "%s", "hello world!");
  assert(!tmk_str_equal(&s1, &s2));

  tmk_str_format(&s2, "%s", "hello");
  assert(tmk_str_equal(&s1, &s2));

  tmk_str_free(&s1);
  tmk_str_free(&s2);
}

void test_equal_cstr(void)
{
  tmk_str s1 = tmk_str_from_cstr("hello");
  char s2[] = "hallo";
  assert(!tmk_str_equal_cstr(&s1, s2));

  char s3[] = "hello world";
  assert(!tmk_str_equal_cstr(&s1, s3));

  char s4[] = "hello";
  assert(tmk_str_equal_cstr(&s1, s4));

  tmk_str_free(&s1);
}

void test_equal_nocase(void)
{
  tmk_str s1 = tmk_str_from_cstr("hello");
  tmk_str s2 = tmk_str_from_cstr("HELLO");
  assert(tmk_str_equal_nocase(&s1, &s2));

  tmk_str_free(&s1);
  tmk_str_free(&s2);
}

void test_tokenize(void)
{
  tmk_str s = tmk_str_from_cstr("a b c");
  tmk_str* t;
  size_t n_tok = tmk_str_tokenize(&t, &s, ' ');
  assert(n_tok == 3);
  assert(strcmp(t[0].str, "a") == 0);
  assert(strcmp(t[1].str, "b") == 0);
  assert(strcmp(t[2].str, "c") == 0);
  for (size_t i = 0; i < n_tok; ++i)
  {
    tmk_str_free(&t[i]);
  }
  free(t);

  tmk_str_format(&s, "  trailing  spaces  ");
  n_tok = tmk_str_tokenize(&t, &s, ' ');
  assert(n_tok == 2);
  assert(strcmp(t[0].str, "trailing") == 0);
  assert(strcmp(t[1].str, "spaces") == 0);
  for (size_t i = 0; i < n_tok; ++i)
  {
    tmk_str_free(&t[i]);
  }
  tmk_str_free(&s);
  free(t);
}

void test_find_char(void)
{
  tmk_str s = tmk_str_from_cstr("hello world");
  size_t pos = tmk_str_find_char(&s, 'l');
  assert(pos == 2);
  size_t pos2 = tmk_str_find_char(&s, 'z');
  assert(pos2 == s.len);
  tmk_str_free(&s);
}

void test_rfind_char(void)
{
  tmk_str s = tmk_str_from_cstr("hello world");
  size_t pos = tmk_str_rfind_char(&s, 'l');
  assert(pos == 9);
  size_t pos2 = tmk_str_rfind_char(&s, 'z');
  assert(pos2 == s.len);
  size_t pos3 = tmk_str_rfind_char(&s, 'h');
  assert(pos3 == 0);
  tmk_str_free(&s);

  tmk_str e = tmk_str_create();
  assert(tmk_str_rfind_char(&e, 'a') == 0);
  tmk_str_free(&e);
}

void test_read_line(void)
{
  FILE* f = tmpfile();
  assert(f != NULL);

  char long_line[200];
  memset(long_line, 'a', sizeof long_line - 1);
  long_line[sizeof long_line - 1] = '\0';

  fprintf(f, "first\n\n%s\nlast without newline", long_line);
  rewind(f);

  /* Small capacity so that the long line forces getline to reallocate. */
  tmk_str s = tmk_str_create_with_capacity(4);

  assert(tmk_str_read_line(&s, f) == 6);
  check_invariants(&s);
  assert(strcmp(s.str, "first\n") == 0);

  assert(tmk_str_read_line(&s, f) == 1);
  check_invariants(&s);
  assert(strcmp(s.str, "\n") == 0);

  assert(tmk_str_read_line(&s, f) == (ssize_t)sizeof long_line);
  check_invariants(&s);
  assert(s.capacity > s.len);
  assert(strncmp(s.str, long_line, sizeof long_line - 1) == 0);
  assert(s.str[s.len - 1] == '\n');

  assert(tmk_str_read_line(&s, f) == 20);
  check_invariants(&s);
  assert(strcmp(s.str, "last without newline") == 0);

  assert(tmk_str_read_line(&s, f) == -1);

  tmk_str_free(&s);
  fclose(f);
}
