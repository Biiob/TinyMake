#ifndef TMK_RUNNER_H
#define TMK_RUNNER_H

#include <poll.h>
#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>

#include "tmk_str.h"

typedef enum
{
  TMK_RUNNER_STATUS_SUCCEEDED,
  TMK_RUNNER_STATUS_FAILED
} tmk_runner_status;

typedef struct
{
  // NULL-terminated. argv[0] is looked up in PATH
  char** argv;
  // Shown with the output of the command when it succeeds
  const tmk_str* display;
  // Identifies the command in error messages
  const tmk_str* name;
  // Not used by the runner: lets the caller match the result to its request
  size_t id;
} tmk_runner_command;

// The fields of the two types below are private to the runner

typedef enum
{
  TMK_RUNNER_JOB_AVAILABLE,
  TMK_RUNNER_JOB_RUNNING
} tmk_runner_job_state;

typedef struct
{
  tmk_runner_command command;
  tmk_runner_job_state state;
  pid_t pid;
  // Everything the command writes to stdout and stderr
  tmk_str output;
} tmk_runner_job;

typedef struct
{
  // One slot per job that can run at the same time
  tmk_runner_job* jobs;
  // pfds[i] is the pipe of jobs[i] while it runs, -1 otherwise
  struct pollfd* pfds;
  size_t max_jobs;
  size_t n_running;
} tmk_runner;

typedef struct
{
  // The id of the command
  size_t id;
  tmk_runner_status status;
} tmk_runner_result;

// max_jobs must be at least 1
void tmk_runner_init(tmk_runner* r, size_t max_jobs);

bool tmk_runner_is_full(const tmk_runner* r);

size_t tmk_runner_n_running(const tmk_runner* r);

// Starts cmd, which the runner must not be full for
// Logs the error and returns false if the process could not be started
bool tmk_runner_start(tmk_runner* r, const tmk_runner_command* cmd);

// Waits until one of the running commands ends, and reports its result
// Returns false if nothing is running, or if waiting failed
bool tmk_runner_wait_any(tmk_runner* r, tmk_runner_result* result);

// Waits for the commands still running, so that no process is left behind
void tmk_runner_free(tmk_runner* r);

#endif
