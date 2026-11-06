#include "tmk_path.h"

#include "tmk_str.h"

tmk_str tmk_path_without_ext(const tmk_str* path)
{
  size_t dot = tmk_str_rfind_char(path, '.');
  size_t slash = tmk_str_rfind_char(path, '/');
  if (slash != path->len && dot < slash)
  {
    return tmk_str_clone(path);
  }

  return tmk_str_substr(path, 0, dot);
}

tmk_str tmk_path_get_dir(const tmk_str* path)
{
  size_t slash = tmk_str_rfind_char(path, '/');

  return slash == path->len ? tmk_str_create() : tmk_str_substr(path, 0, slash);
}
