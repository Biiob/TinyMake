#ifndef TMK_FS_H
#define TMK_FS_H

#include <stdbool.h>
#include <sys/stat.h>

int create_work_directories();
bool is_newer(const struct stat* a, const struct stat* b);

#endif
