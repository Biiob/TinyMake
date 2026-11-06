#ifndef TMK_CLI_PARSER_H
#define TMK_CLI_PARSER_H

#include <stddef.h>

typedef struct
{
  size_t n_processes;
} command_line_arguments;

int parse_arguments(int argc, char* argv[], command_line_arguments* ctx);

#endif
