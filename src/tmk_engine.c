#include "tmk_engine.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "tmk_cmdlog.h"
#include "tmk_depfile.h"
#include "tmk_fs.h"
#include "tmk_log.h"
#include "tmk_mem.h"
#include "tmk_path.h"
#include "tmk_runner.h"
#include "tmk_str.h"
#include "tmk_str_array.h"

static char** build_argv(int argc, const build_config* config,
                         const compile_file* file)
{
  char** argv = tmk_xrealloc(NULL, argc * sizeof(char*));
  size_t arg = 0;
  argv[arg++] = config->compiler.str;
  for (size_t i = 0; i < config->compile_flags.len; ++i)
  {
    argv[arg++] = config->compile_flags.items[i].str;
  }
  for (size_t i = 0; i < config->includes_dir.len; ++i)
  {
    argv[arg++] = "-I";
    argv[arg++] = config->includes_dir.items[i].str;
  }
  argv[arg++] = "-fdiagnostics-color=always";
  argv[arg++] = "-MMD";
  argv[arg++] = "-MF";
  argv[arg++] = file->d_path.str;
  argv[arg++] = "-c";
  argv[arg++] = file->c_path.str;
  argv[arg++] = "-o";
  argv[arg++] = file->o_path.str;
  argv[arg] = NULL;

  return argv;
}

static tmk_str build_display_command(char** argv, size_t argc)
{
  tmk_str command = tmk_str_create();
  for (size_t i = 0; i < argc; ++i)
  {
    if (argv[i] != NULL && argv[i][0] != '\0')
    {
      tmk_str_append_format(&command, "%s ", argv[i]);
    }
  }

  tmk_str_trim(&command);

  return command;
}

static bool up_to_date(const tmk_str* output, const tmk_str_array* inputs,
                       const tmk_str* command, const tmk_cmdlog* cmdlog)
{
  struct stat out_stat;

  // Output does not exist
  if (stat(tmk_str_cstr(output), &out_stat) != 0)
  {
    return false;
  }

  for (size_t i = 0; i < inputs->len; ++i)
  {
    struct stat in_stat;
    // Input does not exist
    if (stat(tmk_str_cstr(&inputs->items[i]), &in_stat) != 0)
    {
      log_msg(DBG, "Could not find or access the input file: %s",
              tmk_str_cstr(&inputs->items[i]));
      return false;
    }

    // Input is newer than output
    if (tmk_fs_is_newer(&in_stat, &out_stat))
    {
      return false;
    }
  }

  // Command is already present in the command log
  const tmk_str* recorded = tmk_cmdlog_find(cmdlog, output);
  bool is_command_recorded = recorded != NULL &&
                             tmk_str_equal(command, recorded);

  return is_command_recorded;
}

// The object is up to date if the depfile exists, and neither the source, the
// headers it lists nor the command changed
static bool object_up_to_date(const compile_file* file, const tmk_str* command,
                              const tmk_cmdlog* cmdlog)
{
  tmk_str_array inputs = {0};
  if (parse_depfile(&inputs, &file->d_path) == -1)
  {
    log_msg(DBG, "Could not read dependency file: %s. Recompiling...",
            tmk_str_cstr(&file->d_path));
    tmk_str_array_free(&inputs);
    return false;
  }
  tmk_str_array_push(&inputs, tmk_str_clone(&file->c_path));

  bool result = up_to_date(&file->o_path, &inputs, command, cmdlog);
  tmk_str_array_free(&inputs);

  return result;
}

// Records the commands that succeeded, so that they are not run again while
// nothing changes
static void record_succeeded(tmk_cmdlog* cmdlog,
                             const tmk_runner_command* commands,
                             size_t n_commands)
{
  for (size_t i = 0; i < n_commands; ++i)
  {
    if (commands[i].status == TMK_RUNNER_STATUS_SUCCEEDED)
    {
      tmk_cmdlog_record(cmdlog, commands[i].output, &commands[i].display);
    }
  }
}

int generate_object_files(const build_config* config, build_state* state,
                          const target* target)
{
  size_t n_inputs = target->source_files_count;
  size_t argc = config->compile_flags.len + 2 * config->includes_dir.len + 10;
  tmk_runner_command_array commands = {0};
  int rc = 0;

  for (size_t i = 0; i < n_inputs; ++i)
  {
    const compile_file* file = &target->source_files[i];
    char** argv = build_argv(argc, config, file);
    tmk_str display = build_display_command(argv, argc);

    if (object_up_to_date(file, &display, &state->cmdlog))
    {
      log_msg(INFO, "Skipping compilation of: %s", tmk_str_cstr(&file->c_path));
      free(argv);
      tmk_str_free(&display);
      continue;
    }

    tmk_str out_dir = tmk_path_get_dir(&file->o_path);
    bool dir_created = tmk_fs_create_dir(&out_dir);
    tmk_str_free(&out_dir);
    if (!dir_created)
    {
      free(argv);
      tmk_str_free(&display);
      rc = -1;
      goto cleanup;
    }

    tmk_runner_command cmd = {
        .argv = argv,
        .display = display,
        .name = tmk_str_clone(&file->c_path),
        .output = &file->o_path,
    };
    tmk_runner_command_array_push(&commands, cmd);
  }

  tmk_runner_result r = tmk_runner_run(commands.items, commands.len,
                                       state->n_processes);

  record_succeeded(&state->cmdlog, commands.items, commands.len);
  rc = r == TMK_RUNNER_RESULT_FAILED ? -1 : 0;

cleanup:
  tmk_runner_command_array_free(&commands);

  return rc;
}

int link_object_files(const build_config* config, build_state* state,
                      const target* target)
{
  size_t argc = config->link_flags.len + target->source_files_count + 4;
  char** argv = tmk_xrealloc(NULL, argc * sizeof(char*));
  size_t arg = 0;
  argv[arg++] = config->compiler.str;
  for (size_t i = 0; i < config->link_flags.len; ++i)
  {
    argv[arg++] = config->link_flags.items[i].str;
  }

  argv[arg++] = "-o";
  argv[arg++] = target->name.str;

  tmk_str_array inputs = {0};
  for (size_t i = 0; i < target->source_files_count; ++i)
  {
    argv[arg++] = target->source_files[i].o_path.str;
    tmk_str_array_push(&inputs, tmk_str_clone(&target->source_files[i].o_path));
  }

  argv[arg] = NULL;

  tmk_str display = build_display_command(argv, argc);

  tmk_runner_result r = TMK_RUNNER_RESULT_SUCCESS;

  if (!up_to_date(&target->name, &inputs, &display, &state->cmdlog))
  {
    tmk_runner_command cmd = {
        .argv = argv,
        .display = display,
        .name = tmk_str_clone(&target->name),
        .output = &target->name,
    };
    r = tmk_runner_run(&cmd, 1, state->n_processes);

    record_succeeded(&state->cmdlog, &cmd, 1);
    tmk_str_free(&cmd.name);
  }
  else
  {
    log_msg(INFO, "Nothing to link. Skipping...");
  }
  tmk_str_array_free(&inputs);
  tmk_str_free(&display);
  free(argv);

  return r == TMK_RUNNER_RESULT_FAILED ? -1 : 0;
}
