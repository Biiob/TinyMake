#include "tmk_path.h"

#include <string.h>

tmk_str get_file_name_without_ext(const tmk_str* full_path)
{
  size_t last_slash = tmk_str_rfind_char(full_path, '/');
  last_slash = last_slash == full_path->len ? 0 : last_slash + 1;
  size_t last_dot = tmk_str_rfind_char(full_path, '.');
  last_dot = last_dot == full_path->len ? last_dot : last_dot - 1;

  return tmk_str_substr(full_path, last_slash, last_dot - last_slash + 1);
}
