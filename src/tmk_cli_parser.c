#include "tmk_cli_parser.h"

#include <string.h>

#include "tmk_log.h"
#include "tmk_types.h"

int parse_arguments(int argc, char* argv[], command_line_arguments* ctx)
{
  for (int i = 1; i < argc; ++i)
  {
    if (strcmp(argv[i], "-j") == 0 && i + 1 < argc)
    {
      ctx->n_processes = (size_t)strtol(argv[i + 1], NULL, 10);
      ++i;
    }
    else
    {
      log_msg(ERROR, "Unknown argument: %s", argv[i]);
      return -1;
    }
  }

  return 0;
}
