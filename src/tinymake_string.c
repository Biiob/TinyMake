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

char* string_at(string_t* str, size_t i)
{
  if (i >= str->len)
  {
    return NULL;
  }

  return &str->str[i];
}

char* string_cstr(string_t* str)
{
  return str->str;
}

string_t* string_append(string_t* dst, const string_t* src)
{
  size_t needed_capacity = dst->len + src->len + 1;

  if (needed_capacity > dst->capacity)
  {
    dst->capacity = allocate_or_reallocate(&dst->str, needed_capacity);
  }

  memcpy(dst->str + dst->len, src->str, src->len);
  dst->len = dst->len + src->len;
  dst->str[dst->len] = '\0';
  return dst;
}

string_t* string_append_cstr(string_t* dst, const char* src)
{
  size_t src_len = strlen(src);
  size_t needed_capacity = dst->len + src_len + 1;

  if (needed_capacity > dst->capacity)
  {
    dst->capacity = allocate_or_reallocate(&dst->str, needed_capacity);
  }

  memcpy(dst->str + dst->len, src, src_len);
  dst->len = dst->len + src_len;
  dst->str[dst->len] = '\0';
  return dst;
}

string_t* string_format(string_t* dst, const char* fmt, ...)
{
  va_list ap, ap2;
  va_start(ap, fmt);
  va_copy(ap2, ap);

  size_t needed_capacity = vsnprintf(NULL, 0, fmt, ap) + 1;
  if (needed_capacity > dst->capacity)
  {
    dst->capacity = allocate_or_reallocate(&dst->str, needed_capacity);
  }
  vsnprintf(dst->str, dst->capacity, fmt, ap2);
  dst->len = needed_capacity - 1;
  va_end(ap);
  va_end(ap2);

  return dst;
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
