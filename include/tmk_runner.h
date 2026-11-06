#ifndef TMK_RUNNER_H
#define TMK_RUNNER_H

#include <stddef.h>

#include "tmk_str.h"

// Outcome of a single command
typedef enum
{
  // Not started yet, or never because an earlier command failed
  TMK_RUNNER_STATUS_NOT_RUN,
  TMK_RUNNER_STATUS_SUCCEEDED,
  TMK_RUNNER_STATUS_FAILED
} tmk_runner_status;

typedef struct
{
  char** argv;
  tmk_str display;
  tmk_str name;
  // File the command creates. Never read by the runner: it is there for the
  // caller to act on the result
  const tmk_str* output;
  // Set by tmk_runner_run
  tmk_runner_status status;
} tmk_runner_command;

typedef struct
{
  tmk_runner_command* items;
  size_t len;
  size_t capacity;
} tmk_runner_command_array;

void tmk_runner_command_array_push(tmk_runner_command_array* arr,
                                   tmk_runner_command cmd);
void tmk_runner_command_array_free(tmk_runner_command_array* arr);

typedef enum
{
  TMK_RUNNER_RESULT_SUCCESS,
  TMK_RUNNER_RESULT_FAILED
} tmk_runner_result;

// Runs the commands, at most parallel_proc at a time, and starts no new one
// after a failure. parallel_proc must not be 0.
// Sets the status of every command, whatever the result
tmk_runner_result tmk_runner_run(tmk_runner_command* commands,
                                 size_t n_commands, size_t parallel_proc);

#endif
