#ifndef TINYMAKE_LOG
#define TINYMAKE_LOG

typedef enum { DEBUG, INFO, WARN, ERROR } log_level;

void log_msg(log_level l, const char* msg, ...);

#endif
