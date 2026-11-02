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

compile_file create_compile_file(tmk_str* c_path)
{
  compile_file cf = {0};
  cf.c_path = *c_path;
  *c_path = (tmk_str){0};
  tmk_str stripped_name = get_file_name_without_ext(&cf.c_path);
  cf.o_path = tmk_str_from_cstr(OBJ_DIR);
  tmk_str_append_format(&cf.o_path, "/%s.o", tmk_str_cstr(&stripped_name));
  cf.d_path = tmk_str_from_cstr(OBJ_DIR);
  tmk_str_append_format(&cf.d_path, "/%s.d", tmk_str_cstr(&stripped_name));
  tmk_str_free(&stripped_name);
  return cf;
}
