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

string_t string_create(void)
{
  string_t str = string_create_with_capacity(DEFAULT_STRING_SIZE);
  return str;
}

string_t string_create_with_capacity(size_t capacity)
{
  capacity = capacity == 0 ? 1 : capacity;

  string_t str;
  str.str = NULL;
  str.capacity = allocate_or_reallocate(&str.str, capacity);
  str.str[0] = '\0';
  str.len = 0;

  return str;
}

void string_reserve(string_t* str, size_t capacity)
{
  if (capacity > str->capacity)
  {
    str->capacity = allocate_or_reallocate(&str->str, capacity);
  }
}

void string_clear(string_t* str)
{
  str->len = 0;
  str->str[0] = '\0';
}

void string_free(string_t* str)
{
  free(str->str);
  str->str = NULL;
  str->len = 0;
  str->capacity = 0;
}

string_t string_clone(const string_t* other)
{
  char* ptr;
  size_t capacity = allocate_or_reallocate(&ptr, other->capacity);
  memcpy(ptr, other->str, other->len);
  ptr[other->len] = '\0';

  string_t str = {
      .str = ptr,
      .len = other->len,
      .capacity = capacity,
  };

  return str;
}

string_t string_from_cstr(const char* other)
{
  size_t other_len = strlen(other);
  size_t needed_capacity = other_len + 1;
  char* ptr;
  size_t capacity = allocate_or_reallocate(&ptr, needed_capacity);
  memcpy(ptr, other, other_len);
  ptr[other_len] = '\0';

  string_t str = {
      .str = ptr,
      .len = other_len,
      .capacity = capacity,
  };

  return str;
}

string_t string_take(char* s)
{
  size_t len = strlen(s);
  string_t str = {
      .str = s,
      .len = len,
      .capacity = len + 1,
  };

  return str;
}

char* string_release(string_t* str)
{
  char* tmp = str->str;
  str->str = NULL;
  str->len = 0;
  str->capacity = 0;

  return tmp;
}

const char* string_at(const string_t* str, size_t i)
{
  if (i >= str->len)
  {
    return NULL;
  }

  return &str->str[i];
}

void string_set(string_t* str, size_t i, char c)
{
  if (i >= str->len || c == '\0')
  {
    return;
  }

  str->str[i] = c;
}

const char* string_cstr(const string_t* str)
{
  return str->str;
}

string_t* string_append_n(string_t* dst, const char* src, size_t n)
{
  size_t capacity = dst->len + n + 1;
  string_reserve(dst, capacity);
  memcpy(dst->str + dst->len, src, n);
  dst->len = dst->len + n;
  dst->str[dst->len] = '\0';

  return dst;
}

string_t* string_append(string_t* dst, const string_t* src)
{
  return string_append_n(dst, src->str, src->len);
}

string_t* string_append_cstr(string_t* dst, const char* src)
{
  return string_append_n(dst, src, strlen(src));
}

string_t* string_push(string_t* dst, char c)
{
  if (c == '\0')
  {
    return dst;
  }

  return string_append_n(dst, &c, 1);
}

string_t* string_vappend_format(string_t* dst, const char* fmt, va_list args)
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
    string_reserve(dst, dst->len + (size_t)n + 1);
    vsnprintf(dst->str + dst->len, (size_t)n + 1, fmt, args);
  }

  dst->len += (size_t)n;
  return dst;
}

string_t* string_append_format(string_t* dst, const char* fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  string_t* s = string_vappend_format(dst, fmt, args);
  va_end(args);

  return s;
}

string_t* string_vformat(string_t* dst, const char* fmt, va_list args)
{
  string_clear(dst);
  return string_vappend_format(dst, fmt, args);
}

string_t* string_format(string_t* dst, const char* fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  string_t* s = string_vformat(dst, fmt, args);
  va_end(args);

  return s;
}

string_t* string_trim(string_t* str)
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

bool string_equal(const string_t* str1, const string_t* str2)
{
  if (str1->len != str2->len)
  {
    return false;
  }

  for (size_t i = 0; i < str1->len; ++i)
  {
    if (*string_at(str1, i) != *string_at(str2, i))
    {
      return false;
    }
  }

  return true;
}

bool string_equal_nocase(const string_t* str1, const string_t* str2)
{
  if (str1->len != str2->len)
  {
    return false;
  }

  for (size_t i = 0; i < str1->len; ++i)
  {
    if (tolower(*string_at(str1, i)) != tolower(*string_at(str2, i)))
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
