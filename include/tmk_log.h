#ifndef TMK_LOG_H
#define TMK_LOG_H

typedef enum
{
  DBG,
  INFO,
  WARN,
  ERROR
} log_level;

void log_msg(log_level l, const char* msg, ...);

#endif
