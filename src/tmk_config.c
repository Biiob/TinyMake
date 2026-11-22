#include "tmk_config.h"

static void free_target(tmk_target* target)
{
  tmk_str_free(&target->name);
  tmk_str_array_free(&target->sources);
  tmk_str_array_free(&target->deps);
}

void tmk_config_free(tmk_config* config)
{
  tmk_str_free(&config->compiler);
  tmk_str_free(&config->archiver);
  tmk_str_array_free(&config->compile_flags);
  tmk_str_array_free(&config->link_flags);
  tmk_str_array_free(&config->include_dirs);
  tmk_array_free_with(&config->targets, free_target);
}
