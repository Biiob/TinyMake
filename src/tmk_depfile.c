#include "tmk_depfile.h"

#include <stdio.h>

ssize_t parse_depfile(tmk_str_array* deps, const tmk_str* path)
{
  *deps = (tmk_str_array){0};

  FILE* f = fopen(tmk_str_cstr(path), "rb");
  if (f == NULL)
  {
    return -1;
  }

  tmk_str line = tmk_str_create();
  bool is_first_line = true;
  while (tmk_str_read_line(&line, f) != -1)
  {
    tmk_str_trim(&line);
    tmk_str_array tokens = tmk_str_tokenize(&line, ' ');
    for (size_t i = 0; i < tokens.len; ++i)
    {
      // First token of the first line is the target
      if (is_first_line && i == 0)
      {
        is_first_line = false;
        continue;
      }
      // gcc's line continuation character
      else if (tokens.items[i].str[0] == '\\')
      {
        continue;
      }

      tmk_str_array_push(deps, tmk_str_array_take(&tokens, i));
    }
    tmk_str_array_free(&tokens);
  }
  tmk_str_free(&line);
  fclose(f);

  return (ssize_t)deps->len;
}
