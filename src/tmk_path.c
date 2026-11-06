#include "tmk_path.h"

#include "tmk_str.h"

tmk_str tmk_path_without_ext(const tmk_str* p)
{
  size_t ldot = tmk_str_rfind_char(p, '.');
  size_t lslash = tmk_str_rfind_char(p, '/');
  if (lslash != p->len && ldot < lslash)
  {
    return tmk_str_clone(p);
  }
  return tmk_str_substr(p, 0, ldot);
}

tmk_str tmk_path_get_dir(const tmk_str* p)
{
  size_t lslash = tmk_str_rfind_char(p, '/');
  return lslash == p->len ? tmk_str_create() : tmk_str_substr(p, 0, lslash);
}

compile_file create_compile_file(tmk_str* c_path)
{
  compile_file cf = {0};
  cf.c_path = *c_path;
  *c_path = (tmk_str){0};

  tmk_str stem = tmk_path_without_ext(&cf.c_path);
  cf.o_path = tmk_str_from_cstr(OBJ_DIR);
  tmk_str_append_format(&cf.o_path, "/%s.o", tmk_str_cstr(&stem));
  cf.d_path = tmk_str_from_cstr(OBJ_DIR);
  tmk_str_append_format(&cf.d_path, "/%s.d", tmk_str_cstr(&stem));
  tmk_str_free(&stem);

  return cf;
}
