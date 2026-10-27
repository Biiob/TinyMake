#include "tmk_parser.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "tmk_log.h"
#include "tmk_path.h"
#include "tmk_str.h"

int parse_build_file(build_context* ctx)
{
  FILE* f = fopen("build.txt", "r");
  if (f == NULL)
  {
    log_msg(ERROR, "Could not open the build file ('build.txt')");
    return -1;
  }

  ctx->targets_count = 0;

  tmk_str line = tmk_str_create_with_capacity(8192);
  while (fgets(tmk_str_data(&line), line.capacity, f))
  {
    tmk_str_set_len(&line, strlen(tmk_str_data(&line)));

    // Commentaires et lignes vides
    if (line.str[0] == '#' || isspace((unsigned char)line.str[0]))
    {
      continue;
    }

    tmk_str_trim(&line);

    tmk_str* tokens;
    size_t n_token = tmk_str_tokenize(&tokens, &line, ' ');

    tmk_str* key = &tokens[0];

    if (tmk_str_equal_cstr(key, "cc"))
    {
      ctx->compiler = tmk_str_take(tmk_str_release(&tokens[2]));
      log_msg(DBG, "Compiler: %s", tmk_str_cstr(&ctx->compiler));
    }
    else if (tmk_str_equal_cstr(key, "cflags"))
    {
      ctx->compile_flags_count = n_token - 2;

      for (size_t i = 2; i < n_token; ++i)
      {
        log_msg(DBG, "Compile flag: %s", tmk_str_cstr(&tokens[i]));
        ctx->compile_flags[i - 2] = tmk_str_take(tmk_str_release(&tokens[i]));
      }
    }
    else if (tmk_str_equal_cstr(key, "ldflags"))
    {
      ctx->link_flags_count = n_token - 2;

      for (size_t i = 2; i < n_token; ++i)
      {
        ctx->link_flags[i - 2] = tmk_str_take(tmk_str_release(&tokens[i]));
      }
    }
    else if (tmk_str_equal_cstr(key, "includes"))
    {
      ctx->includes_dir_count = n_token - 2;
      log_msg(DBG, "%d include directories", ctx->includes_dir_count);

      for (size_t i = 2; i < n_token; ++i)
      {
        log_msg(DBG, "Include directory: %s", tmk_str_cstr(&tokens[i]));
        ctx->includes_dir[i - 2] = tmk_str_create();
        tmk_str_format(&ctx->includes_dir[i - 2], "-I%s",
                       tmk_str_cstr(&tokens[i]));
      }
    }
    else if (tmk_str_equal_cstr(key, "binary"))
    {
      target* current_target = &ctx->targets[ctx->targets_count];
      current_target->type = BINARY;

      current_target->name = tmk_str_from_cstr(BIN_DIR);
      tmk_str_append_format(&current_target->name, "/%s",
                            tmk_str_cstr(&tokens[1]));

      log_msg(DBG, "Found target: %s", tmk_str_cstr(&current_target->name));

      current_target->source_files_count = n_token - 2;
      log_msg(DBG, "with %d files to compile",
              current_target->source_files_count);

      for (size_t i = 2; i < n_token; ++i)
      {
        compile_file* cf = &current_target->source_files[i - 2];
        cf->c_path = tmk_str_take(tmk_str_release(&tokens[i]));
        cf->stripped_name = get_file_name_without_ext(&cf->c_path);
        cf->o_path = tmk_str_from_cstr(OBJ_DIR);
        tmk_str_append_format(&cf->o_path, "/%s.o",
                              tmk_str_cstr(&cf->stripped_name));
        cf->d_path = tmk_str_from_cstr(OBJ_DIR);
        tmk_str_append_format(&cf->d_path, "/%s.d",
                              tmk_str_cstr(&cf->stripped_name));

        log_msg(DBG, "File to compile: %s", tmk_str_cstr(&cf->c_path));
      }
      ctx->targets_count++;
    }
    for (size_t i = 0; i < n_token; ++i)
    {
      tmk_str_free(&tokens[i]);
    }
    free(tokens);
  }
  tmk_str_free(&line);

  fclose(f);
  return 0;
}
