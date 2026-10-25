#include "tmk_log.h"

#include <stdarg.h>
#include <stdio.h>

static const char* level_str[] = {
    "DEBUG",
    "INFO",
    "WARN",
    "ERROR",
};

void log_msg(log_level l, const char* msg, ...)
{
  if (l == DBG)
  {
#ifndef DEBUG
    return;
#endif
  }

  printf("[%s] ", level_str[l]);

  va_list ap;
  va_start(ap, msg);
  vprintf(msg, ap);
  va_end(ap);

  putc('\n', stdout);
}
