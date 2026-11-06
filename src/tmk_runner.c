#include "tmk_runner.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "tmk_log.h"
#include "tmk_mem.h"

extern char** environ;

typedef enum
{
  PENDING,
  RUNNING,
  DONE,
  FAILED
} tmk_runner_job_status;

typedef struct
{
  tmk_runner_command command;
  pid_t pid;
  int pipes[2];
  tmk_str buffer;
  tmk_runner_job_status status;
} tmk_runner_job;

typedef enum
{
  CHILD_OK,
  CHILD_EXIT_FAILURE,
  CHILD_SIGNALED,
  CHILD_WAIT_ERROR
} tmk_runner_child_result;

static tmk_runner_child_result wait_child(pid_t pid, int* detail)
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

static void report_statuses(const tmk_runner_job* jobs, size_t n_jobs,
                            tmk_runner_command* commands)
{
  for (size_t i = 0; i < n_jobs; ++i)
  {
    switch (jobs[i].status)
    {
      case PENDING:
        commands[i].status = TMK_RUNNER_STATUS_NOT_RUN;
        break;
      case DONE:
        commands[i].status = TMK_RUNNER_STATUS_SUCCEEDED;
        break;
      case RUNNING:
      case FAILED:
        commands[i].status = TMK_RUNNER_STATUS_FAILED;
        break;
    }
  }
}

tmk_runner_result tmk_runner_run(tmk_runner_command* commands,
                                 size_t n_commands, size_t parallel_proc)
{
  assert(parallel_proc > 0);

  if (n_commands == 0)
  {
    return TMK_RUNNER_RESULT_SUCCESS;
  }

  tmk_runner_job* jobs = tmk_xrealloc(NULL,
                                      n_commands * sizeof(tmk_runner_job));
  struct pollfd* pfds = tmk_xrealloc(NULL, n_commands * sizeof(struct pollfd));

  for (size_t i = 0; i < n_commands; ++i)
  {
    jobs[i].command = commands[i];
    jobs[i].buffer = (tmk_str){0};
    jobs[i].status = PENDING;
    pfds[i].fd = -1;
    pfds[i].events = 0;
  }

  size_t avail_proc = parallel_proc;
  bool has_failed_job = false;
  for (;;)
  {
    bool has_pending_job = false;
    bool has_running_job = false;
    for (size_t i = 0; i < n_commands; ++i)
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

    while (avail_proc > 0 && has_pending_job && !has_failed_job)
    {
      size_t j = 0;
      while (j < n_commands && jobs[j].status != PENDING)
      {
        j++;
      }

      if (j == n_commands)
      {
        break;
      }

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

      err = posix_spawnp(&jobs[j].pid, jobs[j].command.argv[0], &fa, NULL,
                         jobs[j].command.argv, environ);
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

      avail_proc--;
    }

    if (avail_proc == parallel_proc)
    {
      continue;
    }

    if (poll(pfds, n_commands, -1) == -1)
    {
      if (errno == EINTR)
      {
        continue;
      }
      else
      {
        log_msg(ERROR, "poll: %s", strerror(errno));
        goto exit_error;
      }
    }

    for (size_t i = 0; i < n_commands; ++i)
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
          avail_proc++;

          int detail;
          switch (wait_child(jobs[i].pid, &detail))
          {
            case CHILD_OK:
              jobs[i].status = DONE;
              log_msg(INFO, "%s\n%s", tmk_str_cstr(&jobs[i].command.display),
                      tmk_str_cstr(&jobs[i].buffer));
              break;

            case CHILD_EXIT_FAILURE:
              jobs[i].status = FAILED;
              log_msg(ERROR, "\n%s", tmk_str_cstr(&jobs[i].buffer));
              log_msg(ERROR,
                      "%s: process exited "
                      "with code %d",
                      tmk_str_cstr(&jobs[i].command.name), detail);
              break;

            case CHILD_SIGNALED:
              jobs[i].status = FAILED;
              log_msg(ERROR, "\n%s", tmk_str_cstr(&jobs[i].buffer));
              log_msg(ERROR,
                      "%s: process killed "
                      "by signal %d",
                      tmk_str_cstr(&jobs[i].command.name), detail);
              break;

            case CHILD_WAIT_ERROR:
              jobs[i].status = FAILED;
              log_msg(ERROR, "waitpid for %s: %s",
                      tmk_str_cstr(&jobs[i].command.name),
                      detail ? strerror(detail)
                             : "unexpected child "
                               "status");
              goto exit_error;
          }

          tmk_str_free(&jobs[i].buffer);
        }
      }
    }
  }

  report_statuses(jobs, n_commands, commands);
  free(jobs);
  free(pfds);

  return has_failed_job ? TMK_RUNNER_RESULT_FAILED : TMK_RUNNER_RESULT_SUCCESS;

exit_error:
  for (size_t i = 0; i < n_commands; ++i)
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
  }

  report_statuses(jobs, n_commands, commands);
  free(jobs);
  free(pfds);

  return TMK_RUNNER_RESULT_FAILED;
}

void tmk_runner_command_array_push(tmk_runner_command_array* arr,
                                   tmk_runner_command cmd)
{
  if (arr->len == arr->capacity)
  {
    size_t capacity = arr->capacity == 0 ? 8 : arr->capacity * 2;
    arr->items = tmk_xrealloc(arr->items,
                              capacity * sizeof(tmk_runner_command));
    arr->capacity = capacity;
  }

  arr->items[arr->len] = cmd;
  arr->len++;
}

void tmk_runner_command_array_free(tmk_runner_command_array* arr)
{
  for (size_t i = 0; i < arr->len; ++i)
  {
    free(arr->items[i].argv);
    tmk_str_free(&arr->items[i].display);
    tmk_str_free(&arr->items[i].name);
  }
  free(arr->items);
  *arr = (tmk_runner_command_array){0};
}
