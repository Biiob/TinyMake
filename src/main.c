#include "tmk_cli.h"
#include "tmk_cmdlog.h"
#include "tmk_config.h"
#include "tmk_constants.h"
#include "tmk_engine.h"
#include "tmk_fs.h"
#include "tmk_log.h"
#include "tmk_parser.h"
#include "tmk_str.h"

int main(int argc, char* argv[])
{
  tmk_log_info("Starting build...");

  tmk_cli_args args;
  if (!tmk_cli_parse(&args, argc, argv) || !tmk_fs_init_work_dirs())
  {
    tmk_log_error("Build has ended in error. See above for the details.");
    return -1;
  }

  tmk_engine_state state = {.max_jobs = args.max_jobs};
  tmk_str cmdlog_path = tmk_str_from_cstr(TMK_CMDLOG_FILE);
  tmk_cmdlog_load(&state.cmdlog, &cmdlog_path);

  tmk_config config = {0};
  tmk_str build_file_path = tmk_str_from_cstr(TMK_BUILD_FILE);
  bool ok = tmk_parser_parse(&config, &build_file_path);
  tmk_str_free(&build_file_path);

  for (size_t i = 0; ok && i < config.n_targets; ++i)
  {
    const tmk_target* target = &config.targets[i];
    tmk_log_info("Building target: %s", tmk_str_cstr(&target->path));
    ok = tmk_engine_compile(&config, &state, target) &&
         tmk_engine_link(&config, &state, target);
  }

  if (!tmk_cmdlog_save(&state.cmdlog, &cmdlog_path))
  {
    tmk_log_warn("Command log file was not updated");
  }
  tmk_str_free(&cmdlog_path);
  tmk_cmdlog_free(&state.cmdlog);
  tmk_config_free(&config);

  if (!ok)
  {
    tmk_log_error("Build has ended in error. See above for the details.");
    return -1;
  }

  tmk_log_info("Build has ended");
  return 0;
}
