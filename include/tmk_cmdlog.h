#ifndef TMK_CMDLOG_H
#define TMK_CMDLOG_H

#include <stdbool.h>

#include "tmk_str.h"
#include "tmk_str_array.h"

// The last command that successfully built each output, so that an output is
// rebuilt when its command changes
typedef struct
{
  tmk_str_array outputs;
  tmk_str_array commands;
} tmk_cmdlog;

// log must be zeroed. A log that cannot be read is not an error: its outputs
// are simply missing, which makes them rebuild
void tmk_cmdlog_load(tmk_cmdlog* log, const tmk_str* path);

// Returns NULL if output was never recorded
// The result is invalidated by tmk_cmdlog_record
const tmk_str* tmk_cmdlog_find(const tmk_cmdlog* log, const tmk_str* output);

void tmk_cmdlog_record(tmk_cmdlog* log, const tmk_str* output,
                       const tmk_str* command);

// Writes to a temporary file in the current directory, then renames it to
// path, which must be in the current directory too. On failure, logs the cause
// and returns false, leaving the file at path untouched
bool tmk_cmdlog_save(const tmk_cmdlog* log, const tmk_str* path);

void tmk_cmdlog_free(tmk_cmdlog* log);

#endif
