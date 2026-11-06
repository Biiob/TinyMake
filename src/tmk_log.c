#include "tmk_log.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>

// stdout is not a constant expression, so it cannot initialize g_sink
static bool g_is_sink_set = false;
static FILE* g_sink = NULL;

static void vlog(const char* level, const char* fmt, va_list args)
{
  FILE* out = g_is_sink_set ? g_sink : stdout;
  if (out == NULL)
  {
    return;
  }

  fprintf(out, "[%s] ", level);
  vfprintf(out, fmt, args);
  putc('\n', out);
}

void tmk_log_debug(const char* fmt, ...)
{
#ifdef DEBUG
  va_list args;
  va_start(args, fmt);
  vlog("DEBUG", fmt, args);
  va_end(args);
#else
  (void)fmt;
#endif
}

void tmk_log_info(const char* fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  vlog("INFO", fmt, args);
  va_end(args);
}

void tmk_log_warn(const char* fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  vlog("WARN", fmt, args);
  va_end(args);
}

void tmk_log_error(const char* fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  vlog("ERROR", fmt, args);
  va_end(args);
}

void tmk_log_set_sink(FILE* sink)
{
  g_sink = sink;
  g_is_sink_set = true;
}
