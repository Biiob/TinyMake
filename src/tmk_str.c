#include "tmk_str.h"

#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tmk_mem.h"

#define TMK_STR_MIN_CAP 32

// Sets the capacity to exactly capacity bytes
static void resize(tmk_str* str, size_t capacity)
{
  bool had_buffer = str->str != NULL;
  str->str = tmk_xrealloc(str->str, capacity);
  str->capacity = capacity;
  if (!had_buffer)
  {
    str->str[0] = '\0';
  }
}

static void grow(tmk_str* str, size_t needed)
{
  if (needed <= str->capacity)
  {
    return;
  }

  size_t capacity = str->capacity * 2;
  capacity = capacity < TMK_STR_MIN_CAP ? TMK_STR_MIN_CAP : capacity;
  capacity = capacity < needed ? needed : capacity;
  resize(str, capacity);
}

tmk_str tmk_str_create(void)
{
  return tmk_str_create_with_capacity(TMK_STR_MIN_CAP);
}

tmk_str tmk_str_create_with_capacity(size_t capacity)
{
  capacity = capacity == 0 ? 1 : capacity;

  tmk_str str = {0};
  resize(&str, capacity);

  return str;
}

void tmk_str_reserve(tmk_str* str, size_t capacity)
{
  if (capacity > str->capacity)
  {
    resize(str, capacity);
  }
}

void tmk_str_clear(tmk_str* str)
{
  str->len = 0;
  if (str->str != NULL)
  {
    str->str[0] = '\0';
  }
}

void tmk_str_free(tmk_str* str)
{
  free(str->str);
  str->str = NULL;
  str->len = 0;
  str->capacity = 0;
}

tmk_str tmk_str_clone(const tmk_str* other)
{
  return tmk_str_from_buf(tmk_str_cstr(other), other->len);
}

tmk_str tmk_str_from_buf(const char* buf, size_t n)
{
  tmk_str str = tmk_str_create_with_capacity(n + 1);
  if (n > 0)
  {
    memcpy(str.str, buf, n);
  }
  str.len = n;
  str.str[str.len] = '\0';

  return str;
}

tmk_str tmk_str_from_cstr(const char* cstr)
{
  return tmk_str_from_buf(cstr, strlen(cstr));
}

tmk_str tmk_str_substr(const tmk_str* str, size_t pos, size_t n)
{
  if (pos >= str->len)
  {
    return tmk_str_create();
  }
  if (n > str->len - pos)
  {
    n = str->len - pos;
  }

  return tmk_str_from_buf(str->str + pos, n);
}

tmk_str tmk_str_take(char* cstr)
{
  size_t len = strlen(cstr);
  tmk_str str = {
      .str = cstr,
      .len = len,
      .capacity = len + 1,
  };

  return str;
}

char* tmk_str_release(tmk_str* str)
{
  char* tmp = str->str;
  str->str = NULL;
  str->len = 0;
  str->capacity = 0;

  return tmp;
}

const char* tmk_str_cstr(const tmk_str* str)
{
  return str->str != NULL ? str->str : "";
}

char* tmk_str_data(tmk_str* str)
{
  return str->str;
}

void tmk_str_set_len(tmk_str* str, size_t len)
{
  if (str->str == NULL && len == 0)
  {
    return;
  }

  assert(len < str->capacity);
  str->len = len;
  str->str[len] = '\0';
}

tmk_str* tmk_str_append_n(tmk_str* dst, const char* src, size_t n)
{
  if (n == 0)
  {
    return dst;
  }

  // src may point into dst, so it must be rebased if grow() moves the buffer
  uintptr_t begin = (uintptr_t)dst->str;
  bool aliased = dst->str != NULL && (uintptr_t)src >= begin &&
                 (uintptr_t)src < begin + dst->capacity;
  size_t offset = aliased ? (size_t)((uintptr_t)src - begin) : 0;

  grow(dst, dst->len + n + 1);
  if (aliased)
  {
    src = dst->str + offset;
  }
  memmove(dst->str + dst->len, src, n);
  dst->len = dst->len + n;
  dst->str[dst->len] = '\0';

  return dst;
}

tmk_str* tmk_str_append(tmk_str* dst, const tmk_str* src)
{
  return tmk_str_append_n(dst, src->str, src->len);
}

tmk_str* tmk_str_append_cstr(tmk_str* dst, const char* src)
{
  return tmk_str_append_n(dst, src, strlen(src));
}

tmk_str* tmk_str_push(tmk_str* dst, char c)
{
  if (c == '\0')
  {
    return dst;
  }

  return tmk_str_append_n(dst, &c, 1);
}

tmk_str* tmk_str_vappend_format(tmk_str* dst, const char* fmt, va_list args)
{
  size_t available = dst->capacity - dst->len;
  char* end = dst->str != NULL ? dst->str + dst->len : NULL;
  va_list copy;
  va_copy(copy, args);
  int n = vsnprintf(end, available, fmt, copy);
  va_end(copy);

  if (n < 0)
  {
    if (dst->str != NULL)
    {
      dst->str[dst->len] = '\0';
    }
    return NULL;
  }

  if ((size_t)n >= available)
  {
    grow(dst, dst->len + (size_t)n + 1);
    vsnprintf(dst->str + dst->len, (size_t)n + 1, fmt, args);
  }

  dst->len += (size_t)n;
  return dst;
}

tmk_str* tmk_str_append_format(tmk_str* dst, const char* fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  tmk_str* s = tmk_str_vappend_format(dst, fmt, args);
  va_end(args);

  return s;
}

tmk_str* tmk_str_vformat(tmk_str* dst, const char* fmt, va_list args)
{
  tmk_str_clear(dst);
  return tmk_str_vappend_format(dst, fmt, args);
}

tmk_str* tmk_str_format(tmk_str* dst, const char* fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  tmk_str* s = tmk_str_vformat(dst, fmt, args);
  va_end(args);

  return s;
}

tmk_str* tmk_str_trim(tmk_str* str)
{
  if (str->str == NULL)
  {
    return str;
  }

  size_t start = 0;
  size_t end = str->len;

  while (start < end && isspace((unsigned char)str->str[start]))
  {
    ++start;
  }

  while (end > start && isspace((unsigned char)str->str[end - 1]))
  {
    --end;
  }

  size_t len = end - start;
  if (start > 0)
  {
    memmove(str->str, str->str + start, len);
  }
  str->str[len] = '\0';
  str->len = len;

  return str;
}

bool tmk_str_equal(const tmk_str* str1, const tmk_str* str2)
{
  if (str1->len != str2->len)
  {
    return false;
  }

  return str1->len == 0 || memcmp(str1->str, str2->str, str1->len) == 0;
}

bool tmk_str_equal_cstr(const tmk_str* str1, const char* str2)
{
  size_t len = strlen(str2);
  if (str1->len != len)
  {
    return false;
  }

  return len == 0 || memcmp(str1->str, str2, len) == 0;
}

bool tmk_str_equal_nocase(const tmk_str* str1, const tmk_str* str2)
{
  if (str1->len != str2->len)
  {
    return false;
  }

  for (size_t i = 0; i < str1->len; ++i)
  {
    if (tolower((unsigned char)str1->str[i]) !=
        tolower((unsigned char)str2->str[i]))
    {
      return false;
    }
  }

  return true;
}

size_t tmk_str_find_char(const tmk_str* str, char c)
{
  return tmk_str_find_next_char(str, 0, c);
}

size_t tmk_str_find_next_char(const tmk_str* str, size_t offset, char c)
{
  assert(offset <= str->len);
  if (offset == str->len)
  {
    return str->len;
  }

  const char* found = memchr(str->str + offset, c, str->len - offset);

  return found != NULL ? (size_t)(found - str->str) : str->len;
}

size_t tmk_str_rfind_char(const tmk_str* str, char c)
{
  for (size_t i = str->len; i-- > 0;)
  {
    if (str->str[i] == c)
    {
      return i;
    }
  }

  return str->len;
}

ssize_t tmk_str_read_line(tmk_str* dst, FILE* f)
{
  ssize_t r = getline(&dst->str, &dst->capacity, f);
  if (r != -1)
  {
    dst->len = (size_t)r;
  }
  else if (dst->str != NULL)
  {
    tmk_str_clear(dst);
  }

  return r;
}
