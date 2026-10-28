#include "tmk_log.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>

// stdout is not a constant expression, so it cannot initialize g_sink
static bool g_is_sink_set = false;
static FILE* g_sink = NULL;

static const char* level_str[] = {
    "DEBUG",
    "INFO",
    "WARN",
    "ERROR",
};

void log_msg(log_level l, const char* msg, ...)
{
  va_list args;
  va_start(args, msg);
  log_vmsg(l, msg, args);
  va_end(args);
}

void log_vmsg(log_level l, const char* msg, va_list args)
{
  if (l == DBG)
  {
#ifndef DEBUG
    return;
#endif
  }

  FILE* out = g_is_sink_set ? g_sink : stdout;
  if (out == NULL)
  {
    return;
  }

  fprintf(out, "[%s] ", level_str[l]);
  vfprintf(out, msg, args);
  putc('\n', out);
}

void log_set_sink(FILE* sink)
{
  g_sink = sink;
  g_is_sink_set = true;
}
