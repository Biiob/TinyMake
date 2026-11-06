#ifndef TMK_CLI_H
#define TMK_CLI_H

#include <stdbool.h>
#include <stddef.h>

typedef struct
{
  // Maximum number of commands run at the same time, at least 1
  size_t max_jobs;
} tmk_cli_args;

// Logs the error and returns false on an unknown argument
bool tmk_cli_parse(tmk_cli_args* args, int argc, char* argv[]);

#endif
