#include "tmk_engine.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <spawn.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "tmk_cmdlog.h"
#include "tmk_depfile.h"
#include "tmk_fs.h"
#include "tmk_log.h"
#include "tmk_str.h"
#include "tmk_str_array.h"
#include "tmk_types.h"

extern char** environ;

typedef enum
{
  PENDING,
  SKIPPED,
  RUNNING,
  DONE,
  FAILED
} job_status;

typedef struct
{
  compile_file* file;
  pid_t pid;
  int pipes[2];
  tmk_str command;
  tmk_str buffer;
  job_status status;
} job;

typedef enum
{
  CHILD_OK,
  CHILD_EXIT_FAILURE,
  CHILD_SIGNALED,
  CHILD_WAIT_ERROR
} child_result;

typedef struct
{
  char** argv;
  size_t argc;
  size_t dep_idx;
  size_t file_idx;
  size_t obj_idx;
} compile_args;

static void build_argv(build_context* ctx, compile_args* args)
{
  size_t arg = 0;
  args->argv[arg++] = ctx->compiler.str;
  for (size_t i = 0; i < ctx->compile_flags.len; ++i)
  {
    args->argv[arg++] = ctx->compile_flags.items[i].str;
  }
  for (size_t i = 0; i < ctx->includes_dir.len; ++i)
  {
    args->argv[arg++] = "-I";
    args->argv[arg++] = ctx->includes_dir.items[i].str;
  }
  args->argv[arg++] = "-fdiagnostics-color=always";
  args->argv[arg++] = "-MMD";
  args->argv[arg++] = "-MF";
  args->dep_idx = arg++;
  args->argv[arg++] = "-c";
  args->file_idx = arg++;
  args->argv[arg++] = "-o";
  args->obj_idx = arg++;
  args->argv[arg] = NULL;
}

static tmk_str build_display_command(const compile_args* args)
{
  tmk_str command = tmk_str_create();
  for (size_t i = 0; i < args->argc; ++i)
  {
    if (args->argv[i] != NULL && args->argv[i][0] != '\0')
    {
      tmk_str_append_format(&command, "%s ", args->argv[i]);
    }
  }

  return command;
}

static child_result wait_child(pid_t pid, int* detail)
{
  int status;
  pid_t r;

  do
  {
    r = waitpid(pid, &status, 0);
  } while (r == -1 && errno == EINTR);

  if (r == -1)
  {
    *detail = errno;
    return CHILD_WAIT_ERROR;
  }

  if (WIFEXITED(status))
  {
    *detail = WEXITSTATUS(status);
    return *detail == 0 ? CHILD_OK : CHILD_EXIT_FAILURE;
  }

  if (WIFSIGNALED(status))
  {
    *detail = WTERMSIG(status);
    return CHILD_SIGNALED;
  }

  *detail = 0;
  return CHILD_WAIT_ERROR;
}

static bool should_recompile(compile_file* file, const tmk_cmdlog* cmdlog,
                             const tmk_str* command)
{
  struct stat c_file_stat;
  if (stat(tmk_str_cstr(&file->c_path), &c_file_stat) != 0)
  {
    log_msg(ERROR, "Could not find or access the following file: %s",
            tmk_str_cstr(&file->c_path));
    return false;
  }

  struct stat o_file_stat = {0};

  // If the .o file is missing, recompile
  if (stat(tmk_str_cstr(&file->o_path), &o_file_stat) != 0)
  {
    return true;
  }

  // If the .o file is older than the .c file, recompile
  if (is_newer(&c_file_stat, &o_file_stat))
  {
    return true;
  }

  // If the .d file can't be read or one of the dependency is newer than the .o
  // file, recompile
  tmk_str_array deps;
  if (parse_depfile(&deps, &file->d_path) == -1)
  {
    log_msg(DBG, "Could not read dependency file: %s. Recompiling...",
            tmk_str_cstr(&file->d_path));
    return true;
  }

  bool has_newer_dependency = false;
  for (size_t i = 0; i < deps.len; ++i)
  {
    struct stat dep_stat;
    if (stat(tmk_str_cstr(&deps.items[i]), &dep_stat) == 0 &&
        is_newer(&dep_stat, &o_file_stat))
    {
      has_newer_dependency = true;
      break;
    }
  }
  tmk_str_array_free(&deps);

  // If the command is not already recorded in the command log, recompile
  const tmk_str* recorded = tmk_cmdlog_find(cmdlog, &file->o_path);
  bool is_command_recorded = recorded != NULL &&
                             tmk_str_equal(command, recorded);

  return has_newer_dependency || !is_command_recorded;
}

int generate_object_files(build_context* ctx, target* target)
{
  const size_t n_processes = ctx->n_processes != 0 ? ctx->n_processes : 1;
  size_t available_processes = n_processes;
  const size_t n_files = target->source_files_count;
  job jobs[n_files];
  struct pollfd pfds[n_files];
  for (size_t i = 0; i < n_files; ++i)
  {
    jobs[i].file = &target->source_files[i];
    jobs[i].command = (tmk_str){0};
    jobs[i].buffer = (tmk_str){0};
    jobs[i].status = PENDING;
    pfds[i].fd = -1;
    pfds[i].events = 0;
  }

  size_t argc = ctx->compile_flags.len + 2 * ctx->includes_dir.len + 10;
  char* argv[argc];
  compile_args args = {0};
  args.argc = argc;
  args.argv = argv;
  build_argv(ctx, &args);

  bool has_failed_job = false;
  for (;;)
  {
    bool has_pending_job = false;
    bool has_running_job = false;
    for (size_t i = 0; i < n_files; ++i)
    {
      if (jobs[i].status == FAILED)
      {
        has_failed_job = true;
      }
      if (jobs[i].status == PENDING)
      {
        has_pending_job = true;
      }
      else if (jobs[i].status == RUNNING)
      {
        has_running_job = true;
      }
    }

    if (!has_running_job && (!has_pending_job || has_failed_job))
    {
      break;
    }

    while (available_processes > 0 && has_pending_job && !has_failed_job)
    {
      size_t j = 0;
      while (j < n_files && jobs[j].status != PENDING)
      {
        j++;
      }

      if (j == n_files)
      {
        break;
      }

      args.argv[args.dep_idx] = tmk_str_data(&jobs[j].file->d_path);
      args.argv[args.file_idx] = tmk_str_data(&jobs[j].file->c_path);
      args.argv[args.obj_idx] = tmk_str_data(&jobs[j].file->o_path);

      jobs[j].command = build_display_command(&args);
      tmk_str_trim(&jobs[j].command);

      if (!should_recompile(jobs[j].file, &ctx->cmdlog, &jobs[j].command))
      {
        log_msg(INFO, "Skipping compilation of: %s",
                tmk_str_cstr(&jobs[j].file->c_path));
        jobs[j].status = SKIPPED;
        tmk_str_free(&jobs[j].command);
      }
      else
      {
        if (pipe(jobs[j].pipes) != 0)
        {
          log_msg(ERROR, "pipe: %s", strerror(errno));
          jobs[j].status = FAILED;
          goto exit_error;
        }

        if (fcntl(jobs[j].pipes[0], F_SETFD, FD_CLOEXEC) != 0 ||
            fcntl(jobs[j].pipes[1], F_SETFD, FD_CLOEXEC) != 0)
        {
          log_msg(ERROR, "fcntl: %s", strerror(errno));
          jobs[j].status = FAILED;
          close(jobs[j].pipes[0]);
          close(jobs[j].pipes[1]);
          goto exit_error;
        }

        posix_spawn_file_actions_t fa;
        int err = posix_spawn_file_actions_init(&fa);
        if (err)
        {
          log_msg(ERROR, "posix_spawn_file_actions_init: %s", strerror(err));
          jobs[j].status = FAILED;
          close(jobs[j].pipes[0]);
          close(jobs[j].pipes[1]);
          goto exit_error;
        }

        err = posix_spawn_file_actions_adddup2(&fa, jobs[j].pipes[1],
                                               STDOUT_FILENO);
        if (err)
        {
          log_msg(ERROR, "posix_spawn_file_actions_adddup2: %s", strerror(err));
          jobs[j].status = FAILED;
          close(jobs[j].pipes[0]);
          close(jobs[j].pipes[1]);
          posix_spawn_file_actions_destroy(&fa);
          goto exit_error;
        }

        err = posix_spawn_file_actions_adddup2(&fa, jobs[j].pipes[1],
                                               STDERR_FILENO);
        if (err)
        {
          log_msg(ERROR, "posix_spawn_file_actions_adddup2: %s", strerror(err));
          jobs[j].status = FAILED;
          close(jobs[j].pipes[0]);
          close(jobs[j].pipes[1]);
          posix_spawn_file_actions_destroy(&fa);
          goto exit_error;
        }

        err = posix_spawnp(&jobs[j].pid, tmk_str_cstr(&ctx->compiler), &fa,
                           NULL, argv, environ);
        posix_spawn_file_actions_destroy(&fa);
        if (err)
        {
          log_msg(ERROR, "posix_spawnp: %s", strerror(err));
          jobs[j].status = FAILED;
          close(jobs[j].pipes[0]);
          close(jobs[j].pipes[1]);
          goto exit_error;
        }

        jobs[j].status = RUNNING;
        jobs[j].buffer = tmk_str_create_with_capacity(1024);

        close(jobs[j].pipes[1]);
        pfds[j].fd = jobs[j].pipes[0];
        pfds[j].events = POLLIN;

        available_processes--;
      }
    }

    if (available_processes == n_processes)
    {
      continue;
    }

    if (poll(pfds, n_files, -1) == -1)
    {
      if (errno == EINTR)
      {
        continue;
      }
      else
      {
        goto exit_error;
      }
    }

    for (size_t i = 0; i < n_files; ++i)
    {
      if (jobs[i].status != RUNNING)
      {
        continue;
      }

      if (pfds[i].revents & (POLLIN | POLLHUP | POLLERR))
      {
        char buf[4096];
        ssize_t n = read(pfds[i].fd, buf, sizeof(buf));
        if (n > 0)
        {
          tmk_str_append_n(&jobs[i].buffer, buf, n);
        }
        else if (n == 0 || errno != EINTR)
        {
          close(pfds[i].fd);
          pfds[i].fd = -1;
          available_processes++;

          int detail;
          switch (wait_child(jobs[i].pid, &detail))
          {
            case CHILD_OK:
              jobs[i].status = DONE;
              log_msg(INFO, "%s\n%s", tmk_str_cstr(&jobs[i].command),
                      tmk_str_cstr(&jobs[i].buffer));
              tmk_cmdlog_record(&ctx->cmdlog, &jobs[i].file->o_path,
                                &jobs[i].command);
              break;

            case CHILD_EXIT_FAILURE:
              jobs[i].status = FAILED;
              log_msg(ERROR, "\n%s", tmk_str_cstr(&jobs[i].buffer));
              log_msg(ERROR,
                      "%s: compiler exited "
                      "with code %d",
                      tmk_str_cstr(&jobs[i].file->c_path), detail);
              break;

            case CHILD_SIGNALED:
              jobs[i].status = FAILED;
              log_msg(ERROR, "\n%s", tmk_str_cstr(&jobs[i].buffer));
              log_msg(ERROR,
                      "%s: compiler killed "
                      "by signal %d",
                      tmk_str_cstr(&jobs[i].file->c_path), detail);
              break;

            case CHILD_WAIT_ERROR:
              jobs[i].status = FAILED;
              log_msg(ERROR, "waitpid for %s: %s",
                      tmk_str_cstr(&jobs[i].file->c_path),
                      detail ? strerror(detail)
                             : "unexpected child "
                               "status");
              goto exit_error;
          }

          tmk_str_free(&jobs[i].buffer);
          tmk_str_free(&jobs[i].command);
        }
      }
    }
  }

  return has_failed_job ? -1 : 0;

exit_error:
  for (size_t i = 0; i < n_files; ++i)
  {
    if (jobs[i].status == RUNNING)
    {
      if (pfds[i].fd != -1)
      {
        close(pfds[i].fd);
      }
      pfds[i].fd = -1;
      int d;
      wait_child(jobs[i].pid, &d);
    }

    tmk_str_free(&jobs[i].buffer);
    tmk_str_free(&jobs[i].command);
  }
  return -1;
}

int link_object_files(build_context* ctx, target* target)
{
  tmk_str command = tmk_str_create();
  tmk_str_append(&command, &ctx->compiler);
  for (size_t i = 0; i < ctx->link_flags.len; ++i)
  {
    tmk_str_append_format(&command, " %s", ctx->link_flags.items[i].str);
  }

  tmk_str_append_format(&command, " -o %s", tmk_str_cstr(&target->name));

  for (size_t i = 0; i < target->source_files_count; ++i)
  {
    tmk_str_append_format(&command, " %s",
                          tmk_str_cstr(&target->source_files[i].o_path));
  }

  struct stat s;
  bool target_exists = stat(tmk_str_cstr(&target->name), &s) == 0;
  int rc = 0;

  bool should_relink = false;
  for (size_t i = 0; target_exists && i < target->source_files_count; ++i)
  {
    struct stat file_stat;
    if (stat(tmk_str_cstr(&target->source_files[i].o_path), &file_stat) != 0 ||
        is_newer(&file_stat, &s))
    {
      should_relink = true;
      break;
    }
  }

  const tmk_str* recorded = tmk_cmdlog_find(&ctx->cmdlog, &target->name);
  bool is_command_recorded = recorded != NULL &&
                             tmk_str_equal(&command, recorded);

  if (!target_exists || should_relink || !is_command_recorded)
  {
    log_msg(INFO, "Linking...");
    log_msg(DBG, "Link command: %s", tmk_str_cstr(&command));
    rc = system(tmk_str_cstr(&command));
    if (rc == 0)
    {
      tmk_cmdlog_record(&ctx->cmdlog, &target->name, &command);
    }
  }
  else
  {
    log_msg(INFO, "Nothing to link. Skipping...");
  }

  tmk_str_free(&command);

  return rc;
}
