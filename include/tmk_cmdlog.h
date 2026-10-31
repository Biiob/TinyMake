#ifndef TMK_CMDLOG_H
#define TMK_CMDLOG_H

#include "tmk_str.h"
#include "tmk_str_array.h"

typedef struct
{
  tmk_str_array outputs;
  tmk_str_array commands;
} tmk_cmdlog;

// log must be zeroed. A log that cannot be read is not an error: its outputs
// are simply missing, which makes them rebuild
void tmk_cmdlog_load(tmk_cmdlog* log, const tmk_str* path);

// Returns the command last recorded for output, or NULL if there is none
// The result is invalidated by tmk_cmdlog_record
const tmk_str* tmk_cmdlog_find(const tmk_cmdlog* log, const tmk_str* output);

void tmk_cmdlog_record(tmk_cmdlog* log, const tmk_str* output,
                       const tmk_str* command);

// path must be in the current directory, where the temporary file is created
// On failure, logs the cause and returns -1, leaving the file at path untouched
int tmk_cmdlog_save(const tmk_cmdlog* log, const tmk_str* path);

void tmk_cmdlog_free(tmk_cmdlog* log);

#endif
