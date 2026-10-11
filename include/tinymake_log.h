#ifndef TINYMAKE_LOG
#define TINYMAKE_LOG

typedef enum { DBG, INFO, WARN, ERROR } log_level_t;

void log_msg(log_level_t l, const char* msg, ...);

#endif
