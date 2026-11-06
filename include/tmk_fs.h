#ifndef TMK_FS_H
#define TMK_FS_H

#include <stdbool.h>
#include <sys/stat.h>

#include "tmk_str.h"

// Creates path and its missing parents. An empty path is not an error
// Logs the error and returns false on failure
bool tmk_fs_create_dir(const tmk_str* path);

// Creates TMK_OBJ_DIR and TMK_BIN_DIR
bool tmk_fs_init_work_dirs(void);

// Whether a was modified after b, to the nanosecond
bool tmk_fs_is_newer(const struct stat* a, const struct stat* b);

#endif
