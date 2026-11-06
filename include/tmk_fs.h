#ifndef TMK_FS_H
#define TMK_FS_H

#include <stdbool.h>
#include <sys/stat.h>

#include "tmk_str.h"

bool tmk_fs_create_dir(const tmk_str* p);
bool tmk_fs_init_work_dirs();
bool tmk_fs_is_newer(const struct stat* a, const struct stat* b);

#endif
