#ifndef TMK_RUNNER_H
#define TMK_RUNNER_H

#include "tmk_cmdlog.h"
#include "tmk_str.h"

typedef struct
{
  char** argv;
  tmk_str display;
  tmk_str name;
  tmk_str output;
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

tmk_runner_result tmk_runner_run(const tmk_runner_command* commands,
                                 tmk_cmdlog* cmdlog, size_t n_commands,
                                 int parallel_proc);

#endif
