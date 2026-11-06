#include "tmk_config.h"

#include "tmk_str.h"
#include "tmk_str_array.h"

static void free_target(target* target)
{
  tmk_str_free(&target->name);
  for (size_t i = 0; i < target->source_files_count; ++i)
  {
    tmk_str_free(&target->source_files[i].c_path);
    tmk_str_free(&target->source_files[i].o_path);
    tmk_str_free(&target->source_files[i].d_path);
  }
}

void free_build_config(build_config* config)
{
  tmk_str_free(&config->compiler);
  tmk_str_array_free(&config->compile_flags);
  tmk_str_array_free(&config->link_flags);
  tmk_str_array_free(&config->includes_dir);

  for (size_t i = 0; i < config->targets_count; ++i)
  {
    free_target(&config->targets[i]);
  }
}
