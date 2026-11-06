#include "tmk_fs.h"

#include <errno.h>
#include <string.h>

#include "tmk_constants.h"
#include "tmk_log.h"
#include "tmk_str.h"

static bool create_one_dir(const char* dir)
{
  if (mkdir(dir, 0755) == 0)
  {
    return true;
  }

  int err = errno;
  struct stat s;
  if (err == EEXIST)
  {
    if (stat(dir, &s) == 0 && S_ISDIR(s.st_mode))
    {
      return true;
    }
    err = ENOTDIR;
  }

  log_msg(ERROR, "Could not create directory %s: %s", dir, strerror(err));
  return false;
}

bool tmk_fs_create_dir(const tmk_str* p)
{
  if (p->len == 0)
  {
    return true;
  }

  size_t pos = 1;
  for (;;)
  {
    pos = tmk_str_find_next_char(p, pos, '/');

    tmk_str dir = tmk_str_substr(p, 0, pos);
    bool created = create_one_dir(tmk_str_cstr(&dir));
    tmk_str_free(&dir);

    if (!created)
    {
      return false;
    }

    if (pos == p->len)
    {
      return true;
    }

    pos++;
  }
}

bool tmk_fs_init_work_dirs()
{
  tmk_str obj = tmk_str_from_cstr(OBJ_DIR);
  tmk_str bin = tmk_str_from_cstr(BIN_DIR);

  bool success = tmk_fs_create_dir(&obj) && tmk_fs_create_dir(&bin);
  tmk_str_free(&obj);
  tmk_str_free(&bin);
  return success;
}

bool tmk_fs_is_newer(const struct stat* a, const struct stat* b)
{
  if (a->st_mtim.tv_sec != b->st_mtim.tv_sec)
  {
    return a->st_mtim.tv_sec > b->st_mtim.tv_sec;
  }
  return a->st_mtim.tv_nsec > b->st_mtim.tv_nsec;
}
