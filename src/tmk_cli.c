#include "tmk_cli.h"

#include <stdlib.h>
#include <string.h>

#include "tmk_log.h"

bool tmk_cli_parse(tmk_cli_args* args, int argc, char* argv[])
{
  args->max_jobs = 1;

  for (int i = 1; i < argc; ++i)
  {
    if (strcmp(argv[i], "-j") == 0 && i + 1 < argc)
    {
      // Anything but a positive number keeps the default
      long n = strtol(argv[++i], NULL, 10);
      args->max_jobs = n > 0 ? (size_t)n : 1;
    }
    else
    {
      tmk_log_error("Unknown argument: %s", argv[i]);
      return false;
    }
  }

  return true;
}
