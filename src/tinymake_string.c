#include "tinymake_string.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static size_t allocate_or_reallocate(char** str, size_t capacity)
{
  capacity = capacity < DEFAULT_STRING_SIZE ? DEFAULT_STRING_SIZE : capacity;
  if (*str == NULL)
  {
    char* tmp = malloc(capacity);
    if (tmp == NULL)
    {
      fprintf(stderr, "Could not allocate memory: malloc failed\n");
      abort();
    }
    *str = tmp;
    return capacity;
  }
  else
  {
    char* tmp = realloc(*str, capacity);
    if (tmp == NULL)
    {
      fprintf(stderr, "Could not allocate memory: realloc failed\n");
      abort();
    }
    *str = tmp;
    return capacity;
  }
}

tmk_str tmk_str_create(void)
{
  tmk_str str = tmk_str_create_with_capacity(DEFAULT_STRING_SIZE);
  return str;
}

tmk_str tmk_str_create_with_capacity(size_t capacity)
{
  capacity = capacity == 0 ? 1 : capacity;

  tmk_str str;
  str.str = NULL;
  str.capacity = allocate_or_reallocate(&str.str, capacity);
  str.str[0] = '\0';
  str.len = 0;

  return str;
}

void tmk_str_reserve(tmk_str* str, size_t capacity)
{
  if (capacity > str->capacity)
  {
    str->capacity = allocate_or_reallocate(&str->str, capacity);
  }
}

void tmk_str_clear(tmk_str* str)
{
  str->len = 0;
  str->str[0] = '\0';
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
  char* ptr;
  size_t capacity = allocate_or_reallocate(&ptr, other->capacity);
  memcpy(ptr, other->str, other->len);
  ptr[other->len] = '\0';

  tmk_str str = {
      .str = ptr,
      .len = other->len,
      .capacity = capacity,
  };

  return str;
}

tmk_str tmk_str_from_cstr(const char* other)
{
  size_t other_len = strlen(other);
  size_t needed_capacity = other_len + 1;
  char* ptr;
  size_t capacity = allocate_or_reallocate(&ptr, needed_capacity);
  memcpy(ptr, other, other_len);
  ptr[other_len] = '\0';

  tmk_str str = {
      .str = ptr,
      .len = other_len,
      .capacity = capacity,
  };

  return str;
}

tmk_str tmk_str_take(char* s)
{
  size_t len = strlen(s);
  tmk_str str = {
      .str = s,
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

const char* tmk_str_at(const tmk_str* str, size_t i)
{
  if (i >= str->len)
  {
    return NULL;
  }

  return &str->str[i];
}

void tmk_str_set(tmk_str* str, size_t i, char c)
{
  if (i >= str->len || c == '\0')
  {
    return;
  }

  str->str[i] = c;
}

const char* tmk_str_cstr(const tmk_str* str)
{
  return str->str;
}

tmk_str* tmk_str_append_n(tmk_str* dst, const char* src, size_t n)
{
  size_t capacity = dst->len + n + 1;
  tmk_str_reserve(dst, capacity);
  memcpy(dst->str + dst->len, src, n);
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
  va_list copy;
  va_copy(copy, args);
  int n = vsnprintf(dst->str + dst->len, available, fmt, copy);
  va_end(copy);

  if (n < 0)
  {
    return NULL;
  }

  if ((size_t)n >= available)
  {
    tmk_str_reserve(dst, dst->len + (size_t)n + 1);
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
  char* s = str->str;
  while (*s != '\0' && isspace((unsigned char)*s))
  {
    s++;
  }

  char* e = s;
  if (*s != '\0')
  {
    e = s + strlen(s) - 1;
    while (e > s && isspace((unsigned char)*e))
    {
      e--;
    }
    e[1] = '\0';
  }

  size_t len = *s == '\0' ? 0 : strlen(s);
  char* tmp;
  size_t capacity = allocate_or_reallocate(&tmp, len + 1);
  memcpy(tmp, s, len);
  tmp[len] = '\0';
  free(str->str);
  str->str = tmp;
  str->len = len;
  str->capacity = capacity;

  return str;
}

bool tmk_str_equal(const tmk_str* str1, const tmk_str* str2)
{
  if (str1->len != str2->len)
  {
    return false;
  }

  for (size_t i = 0; i < str1->len; ++i)
  {
    if (*tmk_str_at(str1, i) != *tmk_str_at(str2, i))
    {
      return false;
    }
  }

  return true;
}

bool tmk_str_equal_nocase(const tmk_str* str1, const tmk_str* str2)
{
  if (str1->len != str2->len)
  {
    return false;
  }

  for (size_t i = 0; i < str1->len; ++i)
  {
    if (tolower(*tmk_str_at(str1, i)) != tolower(*tmk_str_at(str2, i)))
    {
      return false;
    }
  }

  return true;
}

char* trim_whitespaces(char* src)
{
  char* s = src;
  while (*s && isspace((unsigned char)*s))
  {
    s++;
  }

  if (*s == '\0')
  {
    return s;
  }

  char* e = s + strlen(s) - 1;
  while (e > s && isspace((unsigned char)*e))
  {
    e--;
  }

  e[1] = '\0';

  return s;
}

size_t tokenize_line(char* line, char* tokens[])
{
  char* save_buffer;
  size_t token_count = 0;
  for (char* token = strtok_r(line, " ", &save_buffer); token != NULL;
       token = strtok_r(NULL, " ", &save_buffer), ++token_count)
  {
    tokens[token_count] = token;
  }

  return token_count;
}
