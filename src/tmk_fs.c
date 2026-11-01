#include "tmk_fs.h"

#include <errno.h>
#include <string.h>

#include "tmk_constants.h"
#include "tmk_log.h"

int create_work_directories()
{
  if ((mkdir(BIN_DIR, 0755) == -1 && errno != EEXIST) ||
      (mkdir(OBJ_DIR, 0755) == -1 && errno != EEXIST))
  {
    log_msg(ERROR, "Could not create work directories: %s", strerror(errno));
    return -1;
  }
  return 0;
}

bool is_newer(const struct stat* a, const struct stat* b)
{
  if (a->st_mtim.tv_sec != b->st_mtim.tv_sec)
  {
    return a->st_mtim.tv_sec > b->st_mtim.tv_sec;
  }
  return a->st_mtim.tv_nsec > b->st_mtim.tv_nsec;
}
