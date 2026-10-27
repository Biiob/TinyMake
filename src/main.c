#include "tmk_cli_parser.h"
#include "tmk_engine.h"
#include "tmk_log.h"
#include "tmk_parser.h"
#include "tmk_types.h"

static void free_target(target* target)
{
  tmk_str_free(&target->name);
  for (size_t i = 0; i < target->source_files_count; ++i)
  {
    tmk_str_free(&target->source_files[i].stripped_name);
    tmk_str_free(&target->source_files[i].c_path);
    tmk_str_free(&target->source_files[i].o_path);
    tmk_str_free(&target->source_files[i].d_path);
  }
}

static void free_context(build_context* ctx)
{
  tmk_str_free(&ctx->compiler);
  for (size_t i = 0; i < ctx->compile_flags_count; ++i)
  {
    tmk_str_free(&ctx->compile_flags[i]);
  }

  for (size_t i = 0; i < ctx->link_flags_count; ++i)
  {
    tmk_str_free(&ctx->link_flags[i]);
  }

  for (size_t i = 0; i < ctx->includes_dir_count; ++i)
  {
    tmk_str_free(&ctx->includes_dir[i]);
  }

  for (size_t i = 0; i < ctx->targets_count; ++i)
  {
    free_target(&ctx->targets[i]);
  }
}

int main(int argc, char* argv[])
{
  log_msg(INFO, "Starting build...");

  command_line_arguments args = {0};
  if (parse_arguments(argc, argv, &args) != 0)
  {
    goto exit_error;
  }

  if (create_needed_directories() != 0)
  {
    goto exit_error;
  }

  if (open_log_file() != 0)
  {
    goto exit_error;
  }

  build_context ctx = {0};
  ctx.n_processes = args.n_processes;
  log_msg(DBG, "Parsing the content of 'build.txt'");
  if (parse_build_file(&ctx) != 0)
  {
    goto cleanup;
  }

  for (size_t i = 0; i < ctx.targets_count; ++i)
  {
    log_msg(INFO, "Building target: %s", tmk_str_cstr(&ctx.targets[i].name));
    if (generate_object_files(&ctx, &ctx.targets[i]) != 0)
    {
      goto cleanup;
    }

    if (link_object_files(&ctx, &ctx.targets[i]) != 0)
    {
      goto cleanup;
    }
  }

  if (close_log_file() != 0)
  {
    goto exit_error;
  }

  free_context(&ctx);
  log_msg(INFO, "Build has ended");
  return 0;

cleanup:
  free_context(&ctx);
  close_log_file();

exit_error:
  log_msg(ERROR, "Build has ended in error. See above for the details.");
  return -1;
}
