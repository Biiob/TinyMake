#include "tmk_engine.h"

#include <stddef.h>
#include <stdio.h>
#include <sys/stat.h>

#include "tmk_cmdlog.h"
#include "tmk_depfile.h"
#include "tmk_fs.h"
#include "tmk_log.h"
#include "tmk_mem.h"
#include "tmk_runner.h"
#include "tmk_str.h"
#include "tmk_str_array.h"
#include "tmk_types.h"

static char** build_argv(int argc, build_context* ctx, compile_file* file)
{
  char** argv = tmk_xrealloc(NULL, argc * sizeof(char*));
  size_t arg = 0;
  argv[arg++] = ctx->compiler.str;
  for (size_t i = 0; i < ctx->compile_flags.len; ++i)
  {
    argv[arg++] = ctx->compile_flags.items[i].str;
  }
  for (size_t i = 0; i < ctx->includes_dir.len; ++i)
  {
    argv[arg++] = "-I";
    argv[arg++] = ctx->includes_dir.items[i].str;
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
    if (is_newer(&in_stat, &out_stat))
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

int generate_object_files(build_context* ctx, target* target)
{
  size_t n_inputs = target->source_files_count;
  size_t argc = ctx->compile_flags.len + 2 * ctx->includes_dir.len + 10;
  tmk_runner_command_array commands = {0};
  for (size_t i = 0; i < n_inputs; ++i)
  {
    char** argv = build_argv(argc, ctx, &target->source_files[i]);
    tmk_str display = build_display_command(argv, argc);

    tmk_str_array inputs = {0};
    bool has_depfile = true;
    if (parse_depfile(&inputs, &target->source_files[i].d_path) == -1)
    {
      log_msg(DBG, "Could not read dependency file: %s. Recompiling...",
              tmk_str_cstr(&target->source_files[i].d_path));
      has_depfile = false;
    }
    tmk_str_array_push(&inputs, tmk_str_clone(&target->source_files[i].c_path));

    if (has_depfile && up_to_date(&target->source_files[i].o_path, &inputs,
                                  &display, &ctx->cmdlog))
    {
      log_msg(INFO, "Skipping compilation of: %s",
              tmk_str_cstr(&target->source_files[i].c_path));
      free(argv);
      tmk_str_free(&display);
    }
    else
    {
      tmk_runner_command cmd = {
          .argv = argv,
          .display = display,
          .name = tmk_str_clone(&target->source_files[i].c_path),
          .output = tmk_str_clone(&target->source_files[i].o_path),
      };
      tmk_runner_command_array_push(&commands, cmd);
    }
    tmk_str_array_free(&inputs);
  }

  tmk_runner_result r = tmk_runner_run(commands.items, &ctx->cmdlog,
                                       commands.len, ctx->n_processes);

  tmk_runner_command_array_free(&commands);

  return r == TMK_RUNNER_RESULT_FAILED ? -1 : 0;
}

int link_object_files(build_context* ctx, target* target)
{
  size_t argc = ctx->link_flags.len + target->source_files_count + 4;
  char** argv = tmk_xrealloc(NULL, argc * sizeof(char*));
  size_t arg = 0;
  argv[arg++] = ctx->compiler.str;
  for (size_t i = 0; i < ctx->link_flags.len; ++i)
  {
    argv[arg++] = ctx->link_flags.items[i].str;
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

  if (!up_to_date(&target->name, &inputs, &display, &ctx->cmdlog))
  {
    tmk_runner_command cmd = {
        .argv = argv,
        .display = display,
        .name = tmk_str_clone(&target->name),
        .output = tmk_str_clone(&target->name),
    };
    r = tmk_runner_run(&cmd, &ctx->cmdlog, 1, ctx->n_processes);

    tmk_str_free(&cmd.name);
    tmk_str_free(&cmd.output);
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
