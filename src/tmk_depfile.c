#include "tmk_depfile.h"

#include <stdio.h>

bool tmk_depfile_parse(tmk_str_array* deps, const tmk_str* path)
{
  *deps = (tmk_str_array){0};

  FILE* f = fopen(tmk_str_cstr(path), "rb");
  if (f == NULL)
  {
    return false;
  }

  tmk_str line = tmk_str_create();
  bool is_target = true;
  while (tmk_str_read_line(&line, f) != -1)
  {
    tmk_str_trim(&line);
    tmk_str_array tokens = tmk_str_tokenize(&line, ' ');
    for (size_t i = 0; i < tokens.len; ++i)
    {
      // The first token is the target, and a lone '\' continues the line
      if (is_target || tokens.items[i].str[0] == '\\')
      {
        is_target = false;
        continue;
      }

      tmk_array_push(deps, tmk_str_array_take(&tokens, i));
    }
    tmk_str_array_free(&tokens);
  }
  tmk_str_free(&line);
  fclose(f);

  return true;
}
