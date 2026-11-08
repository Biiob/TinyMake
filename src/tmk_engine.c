#include "tmk_engine.h"

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

// The arguments point into config and source, which must outlive the result
static char** compile_argv(const tmk_config* config, const tmk_source* source)
{
  // Compiler, flags, "-I <dir>" pairs, 8 fixed arguments and NULL
  size_t argc = config->compile_flags.len + 2 * config->include_dirs.len + 10;
  char** argv = tmk_xrealloc(NULL, argc * sizeof(*argv));

  size_t arg = 0;
  argv[arg++] = config->compiler.str;
  for (size_t i = 0; i < config->compile_flags.len; ++i)
  {
    argv[arg++] = config->compile_flags.items[i].str;
  }
  for (size_t i = 0; i < config->include_dirs.len; ++i)
  {
    argv[arg++] = "-I";
    argv[arg++] = config->include_dirs.items[i].str;
  }
  argv[arg++] = "-fdiagnostics-color=always";
  argv[arg++] = "-MMD";
  argv[arg++] = "-MF";
  argv[arg++] = source->d_path.str;
  argv[arg++] = "-c";
  argv[arg++] = source->c_path.str;
  argv[arg++] = "-o";
  argv[arg++] = source->o_path.str;
  argv[arg] = NULL;

  return argv;
}

// The arguments point into config and target, which must outlive the result
static char** link_argv(const tmk_config* config, const tmk_target* target)
{
  // Compiler, flags, "-o <path>", objects and NULL
  size_t argc = config->link_flags.len + target->n_sources + 4;
  char** argv = tmk_xrealloc(NULL, argc * sizeof(*argv));

  size_t arg = 0;
  argv[arg++] = config->compiler.str;
  for (size_t i = 0; i < config->link_flags.len; ++i)
  {
    argv[arg++] = config->link_flags.items[i].str;
  }
  argv[arg++] = "-o";
  argv[arg++] = target->path.str;
  for (size_t i = 0; i < target->n_sources; ++i)
  {
    argv[arg++] = target->sources[i].o_path.str;
  }
  argv[arg] = NULL;

  return argv;
}

// The command line as shown to the user and recorded in the command log
static tmk_str join_argv(char* const* argv)
{
  tmk_str line = tmk_str_create();
  for (size_t i = 0; argv[i] != NULL; ++i)
  {
    tmk_str_append_format(&line, i == 0 ? "%s" : " %s", argv[i]);
  }

  return line;
}

// output is up to date if it exists, is newer than all of its inputs, and was
// built by the same command
static bool up_to_date(const tmk_str* output, const tmk_str_array* inputs,
                       const tmk_str* command, const tmk_cmdlog* cmdlog)
{
  struct stat out_stat;
  if (stat(tmk_str_cstr(output), &out_stat) != 0)
  {
    return false;
  }

  for (size_t i = 0; i < inputs->len; ++i)
  {
    struct stat in_stat;
    if (stat(tmk_str_cstr(&inputs->items[i]), &in_stat) != 0)
    {
      tmk_log_debug("Could not find or access the input file: %s",
                    tmk_str_cstr(&inputs->items[i]));
      return false;
    }
    if (tmk_fs_is_newer(&in_stat, &out_stat))
    {
      return false;
    }
  }

  const tmk_str* recorded = tmk_cmdlog_find(cmdlog, output);
  return recorded != NULL && tmk_str_equal(command, recorded);
}

// The inputs of an object are its source and the headers its depfile lists
static bool object_up_to_date(const tmk_source* source, const tmk_str* command,
                              const tmk_cmdlog* cmdlog)
{
  tmk_str_array inputs;
  if (!tmk_depfile_parse(&inputs, &source->d_path))
  {
    tmk_log_debug("Could not read dependency file: %s",
                  tmk_str_cstr(&source->d_path));
    return false;
  }
  tmk_array_push(&inputs, tmk_str_clone(&source->c_path));

  bool result = up_to_date(&source->o_path, &inputs, command, cmdlog);
  tmk_str_array_free(&inputs);

  return result;
}

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

bool tmk_engine_compile(const tmk_config* config, tmk_engine_state* state,
                        const tmk_target* target)
{
  tmk_runner_command_array commands = {0};
  bool ok = true;

  for (size_t i = 0; ok && i < target->n_sources; ++i)
  {
    const tmk_source* source = &target->sources[i];
    tmk_runner_command cmd = {
        .argv = compile_argv(config, source),
        .output = &source->o_path,
    };
    cmd.display = join_argv(cmd.argv);

    if (object_up_to_date(source, &cmd.display, &state->cmdlog))
    {
      tmk_log_info("Skipping compilation of: %s",
                   tmk_str_cstr(&source->c_path));
      tmk_runner_command_free(&cmd);
      continue;
    }

    tmk_str dir = tmk_path_get_dir(&source->o_path);
    ok = tmk_fs_create_dir(&dir);
    tmk_str_free(&dir);

    cmd.name = tmk_str_clone(&source->c_path);
    tmk_array_push(&commands, cmd);
  }

  if (ok)
  {
    ok = tmk_runner_run(commands.items, commands.len, state->max_jobs);
    record_succeeded(&state->cmdlog, commands.items, commands.len);
  }
  tmk_array_free_with(&commands, tmk_runner_command_free);

  return ok;
}

bool tmk_engine_link(const tmk_config* config, tmk_engine_state* state,
                     const tmk_target* target)
{
  tmk_runner_command cmd = {
      .argv = link_argv(config, target),
      .name = tmk_str_clone(&target->path),
      .output = &target->path,
  };
  cmd.display = join_argv(cmd.argv);

  tmk_str_array inputs = {0};
  for (size_t i = 0; i < target->n_sources; ++i)
  {
    tmk_array_push(&inputs, tmk_str_clone(&target->sources[i].o_path));
  }

  bool ok = true;
  if (up_to_date(&target->path, &inputs, &cmd.display, &state->cmdlog))
  {
    tmk_log_info("Nothing to link. Skipping...");
  }
  else
  {
    ok = tmk_runner_run(&cmd, 1, state->max_jobs);
    record_succeeded(&state->cmdlog, &cmd, 1);
  }

  tmk_str_array_free(&inputs);
  tmk_runner_command_free(&cmd);

  return ok;
}
