#include "tmk_fs.h"

#include <errno.h>
#include <string.h>

#include "tmk_constants.h"
#include "tmk_log.h"
#include "tmk_str.h"

// Creates a single directory, accepting one that already exists
static bool create_one_dir(const char* dir)
{
  if (mkdir(dir, 0755) == 0)
  {
    return true;
  }

  if (errno == EEXIST)
  {
    struct stat s;
    if (stat(dir, &s) == 0 && S_ISDIR(s.st_mode))
    {
      return true;
    }
    errno = ENOTDIR;
  }

  return false;
}

bool tmk_fs_create_dir(const tmk_str* path)
{
  if (path->len == 0)
  {
    return true;
  }

  // Each '/' ends a parent to create first. Starting at 1 skips the root of an
  // absolute path
  size_t pos = 1;
  for (;;)
  {
    pos = tmk_str_find_next_char(path, pos, '/');

    tmk_str dir = tmk_str_substr(path, 0, pos);
    bool created = create_one_dir(tmk_str_cstr(&dir));
    int err = errno;
    tmk_str_free(&dir);
    errno = err;

    if (!created)
    {
      return false;
    }
    if (pos == path->len)
    {
      return true;
    }

    pos++;
  }
}

// Logs the error on failure
static bool create_work_dir(const char* dir)
{
  tmk_str path = tmk_str_from_cstr(dir);
  bool ok = tmk_fs_create_dir(&path);
  if (!ok)
  {
    tmk_log_error("Could not create directory %s: %s", dir, strerror(errno));
  }
  tmk_str_free(&path);

  return ok;
}

bool tmk_fs_init_work_dirs(void)
{
  return create_work_dir(TMK_OBJ_DIR) && create_work_dir(TMK_BIN_DIR);
}

bool tmk_fs_is_newer(const struct stat* a, const struct stat* b)
{
  if (a->st_mtim.tv_sec != b->st_mtim.tv_sec)
  {
    return a->st_mtim.tv_sec > b->st_mtim.tv_sec;
  }

  return a->st_mtim.tv_nsec > b->st_mtim.tv_nsec;
}
