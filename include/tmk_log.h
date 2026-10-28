#ifndef TMK_LOG_H
#define TMK_LOG_H

#include <stdarg.h>
#include <stdio.h>

typedef enum
{
  DBG,
  INFO,
  WARN,
  ERROR
} log_level;

void log_msg(log_level l, const char* msg, ...);
void log_vmsg(log_level l, const char* msg, va_list args);

// Sends the messages to sink instead of stdout. NULL discards them
void log_set_sink(FILE* sink);

#endif
