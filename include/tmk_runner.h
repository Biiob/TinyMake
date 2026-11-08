#ifndef TMK_RUNNER_H
#define TMK_RUNNER_H

#include <stdbool.h>
#include <stddef.h>

#include "tmk_array.h"
#include "tmk_str.h"

typedef enum
{
  // Not started, because an earlier command failed
  TMK_RUNNER_STATUS_NOT_RUN,
  TMK_RUNNER_STATUS_SUCCEEDED,
  TMK_RUNNER_STATUS_FAILED
} tmk_runner_status;

typedef struct
{
  // NULL-terminated. argv[0] is looked up in PATH
  char** argv;
  // Shown with the output of the command when it succeeds
  tmk_str display;
  // Identifies the command in error messages
  tmk_str name;
  // Not used by the runner: lets the caller act on the result
  const tmk_str* output;
  // Set by tmk_runner_run
  tmk_runner_status status;
} tmk_runner_command;

// Frees argv, display and name
void tmk_runner_command_free(tmk_runner_command* cmd);

tmk_define_array(tmk_runner_command_array, tmk_runner_command);

// Runs the commands, at most max_jobs at a time, and starts no new one after a
// failure. Sets the status of every command, and returns false if one failed
bool tmk_runner_run(tmk_runner_command* commands, size_t n_commands,
                    size_t max_jobs);

#endif
