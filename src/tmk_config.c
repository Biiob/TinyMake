#include "tmk_config.h"

#include "tmk_path.h"
#include "tmk_str.h"
#include "tmk_str_array.h"

tmk_source tmk_source_create(tmk_str c_path)
{
  tmk_str stem = tmk_path_without_ext(&c_path);

  tmk_source source = {.c_path = c_path};
  source.o_path = tmk_str_from_cstr(TMK_OBJ_DIR);
  tmk_str_append_format(&source.o_path, "/%s.o", tmk_str_cstr(&stem));
  source.d_path = tmk_str_from_cstr(TMK_OBJ_DIR);
  tmk_str_append_format(&source.d_path, "/%s.d", tmk_str_cstr(&stem));
  tmk_str_free(&stem);

  return source;
}

static void free_target(tmk_target* target)
{
  tmk_str_free(&target->path);
  for (size_t i = 0; i < target->n_sources; ++i)
  {
    tmk_str_free(&target->sources[i].c_path);
    tmk_str_free(&target->sources[i].o_path);
    tmk_str_free(&target->sources[i].d_path);
  }
}

void tmk_config_free(tmk_config* config)
{
  tmk_str_free(&config->compiler);
  tmk_str_array_free(&config->compile_flags);
  tmk_str_array_free(&config->link_flags);
  tmk_str_array_free(&config->include_dirs);

  for (size_t i = 0; i < config->n_targets; ++i)
  {
    free_target(&config->targets[i]);
  }
}
