#include "tmk_cmdlog.h"

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "tmk_log.h"
#include "tmk_str.h"

// First line of the file, to recognize logs written in another format
#define TMK_CMDLOG_HEADER "tinymake log v1.0\n"

// Returns log->outputs.len if output is not in the log
static size_t index_of(const tmk_cmdlog* log, const tmk_str* output)
{
  size_t i = 0;
  while (i < log->outputs.len && !tmk_str_equal(output, &log->outputs.items[i]))
  {
    ++i;
  }

  return i;
}

void tmk_cmdlog_load(tmk_cmdlog* log, const tmk_str* path)
{
  FILE* f = fopen(tmk_str_cstr(path), "rb");
  if (f == NULL)
  {
    if (errno != ENOENT)
    {
      tmk_log_warn("Could not read command log '%s': %s. Rebuilding everything",
                   tmk_str_cstr(path), strerror(errno));
    }
    return;
  }

  tmk_str line = tmk_str_create();
  if (tmk_str_read_line(&line, f) != -1 &&
      !tmk_str_equal_cstr(&line, TMK_CMDLOG_HEADER))
  {
    tmk_log_warn(
        "Command log '%s' has an unknown format. Rebuilding everything",
        tmk_str_cstr(path));
    tmk_str_free(&line);
    fclose(f);
    return;
  }

  while (tmk_str_read_line(&line, f) != -1)
  {
    if (line.str[0] == '\n' || line.str[0] == '#')
    {
      continue;
    }

    tmk_str_trim(&line);

    size_t pos = tmk_str_find_char(&line, '\t');
    if (pos == line.len)
    {
      continue;
    }

    tmk_str_array_push(&log->outputs, tmk_str_substr(&line, 0, pos));
    tmk_str_array_push(&log->commands,
                       tmk_str_substr(&line, pos + 1, TMK_STR_NPOS));
  }

  if (ferror(f))
  {
    tmk_log_warn(
        "Could not read command log '%s': %s. Files missing from it will "
        "be rebuilt",
        tmk_str_cstr(path), strerror(errno));
  }

  tmk_str_free(&line);
  fclose(f);
}

const tmk_str* tmk_cmdlog_find(const tmk_cmdlog* log, const tmk_str* output)
{
  size_t i = index_of(log, output);

  return i < log->outputs.len ? &log->commands.items[i] : NULL;
}

void tmk_cmdlog_record(tmk_cmdlog* log, const tmk_str* output,
                       const tmk_str* command)
{
  size_t i = index_of(log, output);
  if (i < log->outputs.len)
  {
    tmk_str_format(&log->commands.items[i], "%s", tmk_str_cstr(command));
    return;
  }

  tmk_str_array_push(&log->outputs, tmk_str_clone(output));
  tmk_str_array_push(&log->commands, tmk_str_clone(command));
}

bool tmk_cmdlog_save(const tmk_cmdlog* log, const tmk_str* path)
{
  char tmp_file[] = ".tinymake_log_XXXXXX";

  int tmp_fd = mkstemp(tmp_file);
  if (tmp_fd == -1)
  {
    tmk_log_warn(
        "Could not create a temporary command log in the current "
        "directory: %s",
        strerror(errno));
    return false;
  }

  FILE* f = fdopen(tmp_fd, "wb");
  if (f == NULL)
  {
    tmk_log_warn("Could not write the temporary command log: %s",
                 strerror(errno));
    close(tmp_fd);
    unlink(tmp_file);
    return false;
  }

  fwrite(TMK_CMDLOG_HEADER, sizeof(char), sizeof(TMK_CMDLOG_HEADER) - 1, f);
  tmk_str s = tmk_str_create();
  for (size_t i = 0; i < log->outputs.len; ++i)
  {
    tmk_str_format(&s, "%s\t%s\n", tmk_str_cstr(&log->outputs.items[i]),
                   tmk_str_cstr(&log->commands.items[i]));
    fwrite(tmk_str_cstr(&s), sizeof(char), s.len, f);
  }
  tmk_str_free(&s);

  if (fflush(f) != 0 || fsync(tmp_fd) != 0)
  {
    tmk_log_warn("Could not write the temporary command log: %s",
                 strerror(errno));
    fclose(f);
    unlink(tmp_file);
    return false;
  }

  if (fclose(f) != 0)
  {
    tmk_log_warn("Could not write the temporary command log: %s",
                 strerror(errno));
    unlink(tmp_file);
    return false;
  }

  if (rename(tmp_file, tmk_str_cstr(path)) != 0)
  {
    tmk_log_warn("Could not replace command log '%s': %s", tmk_str_cstr(path),
                 strerror(errno));
    unlink(tmp_file);
    return false;
  }

  return true;
}

void tmk_cmdlog_free(tmk_cmdlog* log)
{
  tmk_str_array_free(&log->outputs);
  tmk_str_array_free(&log->commands);
}
