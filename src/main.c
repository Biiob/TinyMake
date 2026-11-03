#include "tmk_cli_parser.h"
#include "tmk_cmdlog.h"
#include "tmk_engine.h"
#include "tmk_fs.h"
#include "tmk_log.h"
#include "tmk_parser.h"
#include "tmk_str.h"
#include "tmk_types.h"

int main(int argc, char* argv[])
{
  log_msg(INFO, "Starting build...");

  command_line_arguments args = {0};
  if (parse_arguments(argc, argv, &args) != 0)
  {
    goto exit_error;
  }

  if (create_work_directories() != 0)
  {
    goto exit_error;
  }

  build_context ctx = {0};
  ctx.n_processes = args.n_processes != 0 ? args.n_processes : 1;
  tmk_str cmdlog_path = tmk_str_from_cstr(".tinymake_log");
  tmk_cmdlog_load(&ctx.cmdlog, &cmdlog_path);

  tmk_str build_file_path = tmk_str_from_cstr("build.txt");
  log_msg(DBG, "Parsing the content of 'build.txt'");
  int rc = parse_build_file(&ctx, &build_file_path);
  tmk_str_free(&build_file_path);
  if (rc != 0)
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

  if (tmk_cmdlog_save(&ctx.cmdlog, &cmdlog_path) != 0)
  {
    log_msg(WARN, "Command log file was not updated");
  }
  tmk_str_free(&cmdlog_path);
  tmk_cmdlog_free(&ctx.cmdlog);
  free_build_context(&ctx);
  log_msg(INFO, "Build has ended");
  return 0;

cleanup:
  if (tmk_cmdlog_save(&ctx.cmdlog, &cmdlog_path) != 0)
  {
    log_msg(WARN, "Command log file was not updated");
  }
  tmk_str_free(&cmdlog_path);
  tmk_cmdlog_free(&ctx.cmdlog);
  free_build_context(&ctx);

exit_error:
  log_msg(ERROR, "Build has ended in error. See above for the details.");
  return -1;
}
