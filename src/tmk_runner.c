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
static bool start_job(tmk_runner_job* j, int* fd)
{
  int pipes[2];
  if (pipe(pipes) != 0)
  {
    tmk_log_error("pipe: %s", strerror(errno));
    return false;
  }

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
    char** argv = j->command.argv;
    err = posix_spawnp(&j->pid, argv[0], &actions, NULL, argv, environ);
  }
  posix_spawn_file_actions_destroy(&actions);
  close(pipes[1]);

  if (err != 0)
  {
    tmk_log_error("%s: could not start %s: %s", tmk_str_cstr(j->command.name),
                  j->command.argv[0], strerror(err));
    close(pipes[0]);
    return false;
  }

  *fd = pipes[0];
  j->output = tmk_str_create();
  return true;
}

// Reaps the process of j once its output is closed, reports the result, and
// sets status. Returns false if the process could not be waited for
static bool finish_job(tmk_runner_job* j, tmk_runner_status* status)
{
  const char* name = tmk_str_cstr(j->command.name);
  const char* output = tmk_str_cstr(&j->output);
  int detail;
  child_result result = wait_child(j->pid, &detail);

  *status = result == CHILD_OK ? TMK_RUNNER_STATUS_SUCCEEDED
                               : TMK_RUNNER_STATUS_FAILED;
  switch (result)
  {
    case CHILD_OK:
      tmk_log_info("%s\n%s", tmk_str_cstr(j->command.display), output);
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

static size_t find_free_slot(const tmk_runner* r)
{
  for (size_t i = 0; i < r->max_jobs; ++i)
  {
    if (r->jobs[i].state == TMK_RUNNER_JOB_AVAILABLE)
    {
      return i;
    }
  }

  return r->max_jobs;
}

void tmk_runner_init(tmk_runner* r, size_t max_jobs)
{
  assert(max_jobs > 0);

  r->max_jobs = max_jobs;
  r->n_running = 0;
  r->jobs = tmk_xrealloc(NULL, max_jobs * sizeof(*r->jobs));
  r->pfds = tmk_xrealloc(NULL, max_jobs * sizeof(*r->pfds));
  for (size_t i = 0; i < max_jobs; ++i)
  {
    r->jobs[i] = (tmk_runner_job){.state = TMK_RUNNER_JOB_AVAILABLE};
    r->pfds[i] = (struct pollfd){.fd = -1, .events = POLLIN};
  }
}

bool tmk_runner_is_full(const tmk_runner* r)
{
  return r->n_running == r->max_jobs;
}

size_t tmk_runner_n_running(const tmk_runner* r)
{
  return r->n_running;
}

bool tmk_runner_start(tmk_runner* r, const tmk_runner_command* cmd)
{
  size_t slot = find_free_slot(r);
  assert(slot < r->max_jobs);

  tmk_runner_job* j = &r->jobs[slot];
  j->command = *cmd;
  if (!start_job(j, &r->pfds[slot].fd))
  {
    return false;
  }

  j->state = TMK_RUNNER_JOB_RUNNING;
  r->n_running++;
  return true;
}

bool tmk_runner_wait_any(tmk_runner* r, tmk_runner_result* result)
{
  if (r->n_running == 0)
  {
    return false;
  }

  for (;;)
  {
    if (poll(r->pfds, r->max_jobs, -1) == -1)
    {
      if (errno == EINTR)
      {
        continue;
      }
      tmk_log_error("poll: %s", strerror(errno));
      return false;
    }

    for (size_t i = 0; i < r->max_jobs; ++i)
    {
      if (r->pfds[i].fd == -1 || r->pfds[i].revents == 0)
      {
        continue;
      }

      tmk_runner_job* j = &r->jobs[i];
      char buf[4096];
      ssize_t n = read(r->pfds[i].fd, buf, sizeof(buf));
      if (n > 0)
      {
        tmk_str_append_n(&j->output, buf, (size_t)n);
        continue;
      }
      if (n == -1 && errno == EINTR)
      {
        continue;
      }

      // The command closed its output, so it has exited or is about to
      close(r->pfds[i].fd);
      r->pfds[i].fd = -1;
      j->state = TMK_RUNNER_JOB_AVAILABLE;
      r->n_running--;

      result->id = j->command.id;
      return finish_job(j, &result->status);
    }
  }
}

void tmk_runner_free(tmk_runner* r)
{
  for (size_t i = 0; i < r->max_jobs; ++i)
  {
    tmk_runner_job* j = &r->jobs[i];
    if (j->state == TMK_RUNNER_JOB_RUNNING)
    {
      close(r->pfds[i].fd);
      int detail;
      wait_child(j->pid, &detail);
      tmk_str_free(&j->output);
    }
  }

  free(r->jobs);
  free(r->pfds);
  *r = (tmk_runner){0};
}
