#include "tmk_str_array.h"

#include <assert.h>

tmk_str tmk_str_array_take(tmk_str_array* arr, size_t i)
{
  assert(i < arr->len);

  tmk_str str = arr->items[i];
  arr->items[i] = (tmk_str){0};

  return str;
}

void tmk_str_array_free(tmk_str_array* arr)
{
  tmk_array_free_with(arr, tmk_str_free);
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
        tmk_array_push(&tokens, tmk_str_from_buf(str->str + start, i - start));
      }
      start = i + 1;
    }
  }

  return tokens;
}
