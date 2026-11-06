#include "tmk_cli_parser.h"
#include "tmk_cmdlog.h"
#include "tmk_config.h"
#include "tmk_engine.h"
#include "tmk_fs.h"
#include "tmk_log.h"
#include "tmk_parser.h"
#include "tmk_str.h"

int main(int argc, char* argv[])
{
  log_msg(INFO, "Starting build...");

  command_line_arguments args = {0};
  if (parse_arguments(argc, argv, &args) != 0 || !tmk_fs_init_work_dirs())
  {
    log_msg(ERROR, "Build has ended in error. See above for the details.");
    return -1;
  }

  build_state state = {
      .n_processes = args.n_processes != 0 ? args.n_processes : 1,
  };
  tmk_str cmdlog_path = tmk_str_from_cstr(".tinymake_log");
  tmk_cmdlog_load(&state.cmdlog, &cmdlog_path);

  build_config config = {0};
  tmk_str build_file_path = tmk_str_from_cstr("build.txt");
  log_msg(DBG, "Parsing the content of 'build.txt'");
  int rc = parse_build_file(&config, &build_file_path);
  tmk_str_free(&build_file_path);

  for (size_t i = 0; rc == 0 && i < config.targets_count; ++i)
  {
    const target* target = &config.targets[i];
    log_msg(INFO, "Building target: %s", tmk_str_cstr(&target->name));
    rc = generate_object_files(&config, &state, target);
    if (rc == 0)
    {
      rc = link_object_files(&config, &state, target);
    }
  }

  if (tmk_cmdlog_save(&state.cmdlog, &cmdlog_path) != 0)
  {
    log_msg(WARN, "Command log file was not updated");
  }
  tmk_str_free(&cmdlog_path);
  tmk_cmdlog_free(&state.cmdlog);
  free_build_config(&config);

  if (rc != 0)
  {
    log_msg(ERROR, "Build has ended in error. See above for the details.");
    return -1;
  }

  log_msg(INFO, "Build has ended");
  return 0;
}
