#include "tmk_runner.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "tmk_log.h"
#include "tmk_mem.h"

extern char** environ;

typedef enum
{
  JOB_PENDING,
  JOB_RUNNING,
  JOB_DONE,
  JOB_FAILED
} job_state;

typedef struct
{
  tmk_runner_command* command;
  job_state state;
  pid_t pid;
  // Everything the command writes to stdout and stderr
  tmk_str output;
} job;

typedef enum
{
  CHILD_OK,
  CHILD_EXIT_FAILURE,
  CHILD_SIGNALED,
  CHILD_WAIT_ERROR
} child_result;

// detail receives the exit code, the signal or errno, depending on the result
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

// Spawns the command of j with its stdout and stderr sent to a pipe, whose
// read end is stored in fd. Logs the error and returns false on failure
static bool start_job(job* j, int* fd)
{
  int pipes[2];
  if (pipe(pipes) != 0)
  {
    tmk_log_error("pipe: %s", strerror(errno));
    return false;
  }

  // Otherwise the next children would inherit the pipe, and keep it open after
  // this command exits
  if (fcntl(pipes[0], F_SETFD, FD_CLOEXEC) != 0 ||
      fcntl(pipes[1], F_SETFD, FD_CLOEXEC) != 0)
  {
    tmk_log_error("fcntl: %s", strerror(errno));
    close(pipes[0]);
    close(pipes[1]);
    return false;
  }

  posix_spawn_file_actions_t actions;
  int err = posix_spawn_file_actions_init(&actions);
  if (err != 0)
  {
    tmk_log_error("posix_spawn_file_actions_init: %s", strerror(err));
    close(pipes[0]);
    close(pipes[1]);
    return false;
  }

  err = posix_spawn_file_actions_adddup2(&actions, pipes[1], STDOUT_FILENO);
  if (err == 0)
  {
    err = posix_spawn_file_actions_adddup2(&actions, pipes[1], STDERR_FILENO);
  }
  if (err == 0)
  {
    char** argv = j->command->argv;
    err = posix_spawnp(&j->pid, argv[0], &actions, NULL, argv, environ);
  }
  posix_spawn_file_actions_destroy(&actions);
  close(pipes[1]);

  if (err != 0)
  {
    tmk_log_error("%s: could not start %s: %s", tmk_str_cstr(&j->command->name),
                  j->command->argv[0], strerror(err));
    close(pipes[0]);
    return false;
  }

  *fd = pipes[0];
  j->output = tmk_str_create();
  return true;
}

// Reaps the process of j once its output is closed, and reports the result
// Returns false if the process could not be waited for
static bool finish_job(job* j)
{
  const char* name = tmk_str_cstr(&j->command->name);
  const char* output = tmk_str_cstr(&j->output);
  int detail;
  child_result result = wait_child(j->pid, &detail);

  j->state = result == CHILD_OK ? JOB_DONE : JOB_FAILED;
  switch (result)
  {
    case CHILD_OK:
      tmk_log_info("%s\n%s", tmk_str_cstr(&j->command->display), output);
      break;
    case CHILD_EXIT_FAILURE:
      tmk_log_error("\n%s", output);
      tmk_log_error("%s: process exited with code %d", name, detail);
      break;
    case CHILD_SIGNALED:
      tmk_log_error("\n%s", output);
      tmk_log_error("%s: process killed by signal %d", name, detail);
      break;
    case CHILD_WAIT_ERROR:
      tmk_log_error("waitpid for %s: %s", name,
                    detail != 0 ? strerror(detail) : "unexpected status");
      break;
  }

  tmk_str_free(&j->output);
  return result != CHILD_WAIT_ERROR;
}

static tmk_runner_status status_of(job_state state)
{
  switch (state)
  {
    case JOB_PENDING:
      return TMK_RUNNER_STATUS_NOT_RUN;
    case JOB_DONE:
      return TMK_RUNNER_STATUS_SUCCEEDED;
    default:
      return TMK_RUNNER_STATUS_FAILED;
  }
}

bool tmk_runner_run(tmk_runner_command* commands, size_t n_commands,
                    size_t max_jobs)
{
  assert(max_jobs > 0);

  if (n_commands == 0)
  {
    return true;
  }

  job* jobs = tmk_xrealloc(NULL, n_commands * sizeof(*jobs));
  // pfds[i] is the pipe of jobs[i] while it runs, ignored by poll otherwise
  struct pollfd* pfds = tmk_xrealloc(NULL, n_commands * sizeof(*pfds));
  for (size_t i = 0; i < n_commands; ++i)
  {
    jobs[i] = (job){.command = &commands[i], .state = JOB_PENDING};
    pfds[i] = (struct pollfd){.fd = -1, .events = POLLIN};
  }

  // Jobs start in order, so the ones from next on are all pending
  size_t next = 0;
  size_t n_running = 0;
  bool failed = false;

  for (;;)
  {
    while (!failed && n_running < max_jobs && next < n_commands)
    {
      job* j = &jobs[next];
      if (start_job(j, &pfds[next].fd))
      {
        j->state = JOB_RUNNING;
        n_running++;
      }
      else
      {
        j->state = JOB_FAILED;
        failed = true;
      }
      next++;
    }

    if (n_running == 0)
    {
      break;
    }

    if (poll(pfds, n_commands, -1) == -1)
    {
      if (errno == EINTR)
      {
        continue;
      }
      tmk_log_error("poll: %s", strerror(errno));
      goto abort;
    }

    for (size_t i = 0; i < n_commands; ++i)
    {
      if (pfds[i].fd == -1 || pfds[i].revents == 0)
      {
        continue;
      }

      char buf[4096];
      ssize_t n = read(pfds[i].fd, buf, sizeof(buf));
      if (n > 0)
      {
        tmk_str_append_n(&jobs[i].output, buf, (size_t)n);
        continue;
      }
      if (n == -1 && errno == EINTR)
      {
        continue;
      }

      // The command closed its output, so it has exited or is about to
      close(pfds[i].fd);
      pfds[i].fd = -1;
      n_running--;
      if (!finish_job(&jobs[i]))
      {
        goto abort;
      }
      failed = failed || jobs[i].state == JOB_FAILED;
    }
  }

  for (size_t i = 0; i < n_commands; ++i)
  {
    commands[i].status = status_of(jobs[i].state);
  }
  free(jobs);
  free(pfds);

  return !failed;

abort:
  // The results are unknown, so whatever was still running has failed
  for (size_t i = 0; i < n_commands; ++i)
  {
    if (jobs[i].state == JOB_RUNNING && pfds[i].fd != -1)
    {
      close(pfds[i].fd);
      int detail;
      wait_child(jobs[i].pid, &detail);
      jobs[i].state = JOB_FAILED;
    }
    tmk_str_free(&jobs[i].output);
    commands[i].status = status_of(jobs[i].state);
  }
  free(jobs);
  free(pfds);

  return false;
}

void tmk_runner_command_free(tmk_runner_command* cmd)
{
  free(cmd->argv);
  tmk_str_free(&cmd->display);
  tmk_str_free(&cmd->name);
}
