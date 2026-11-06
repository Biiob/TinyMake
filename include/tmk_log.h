#ifndef TMK_LOG_H
#define TMK_LOG_H

#include <stdio.h>

// Messages go to stdout, prefixed with their level
// Debug messages are only written when built with -DDEBUG
void tmk_log_debug(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void tmk_log_info(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void tmk_log_warn(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void tmk_log_error(const char* fmt, ...) __attribute__((format(printf, 1, 2)));

// Writes to sink instead of stdout. NULL discards everything
void tmk_log_set_sink(FILE* sink);

#endif
