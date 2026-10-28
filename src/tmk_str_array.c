#include "tmk_str_array.h"

#include <stdlib.h>

#include "tmk_mem.h"

#define TMK_STR_ARRAY_MIN_CAP 8

void tmk_str_array_push(tmk_str_array* arr, tmk_str str)
{
  if (arr->len == arr->capacity)
  {
    size_t capacity = arr->capacity == 0 ? TMK_STR_ARRAY_MIN_CAP
                                         : arr->capacity * 2;
    arr->items = tmk_xrealloc(arr->items, capacity * sizeof(tmk_str));
    arr->capacity = capacity;
  }

  arr->items[arr->len] = str;
  arr->len++;
}

void tmk_str_array_free(tmk_str_array* arr)
{
  for (size_t i = 0; i < arr->len; ++i)
  {
    tmk_str_free(&arr->items[i]);
  }
  free(arr->items);
  *arr = (tmk_str_array){0};
}

tmk_str_array tmk_str_tokenize(const tmk_str* str, char delim)
{
  tmk_str_array tokens = {0};
  size_t start = 0;

  for (size_t i = 0; i <= str->len; ++i)
  {
    if (i == str->len || str->str[i] == delim)
    {
      if (i > start)
      {
        tmk_str_array_push(&tokens,
                           tmk_str_from_buf(str->str + start, i - start));
      }
      start = i + 1;
    }
  }

  return tokens;
}
