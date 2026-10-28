#include "test_util.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

tmk_str test_write_temp_file(const char* content)
{
  char name[] = "/tmp/tmk_test_XXXXXX";
  int fd = mkstemp(name);
  assert(fd != -1);

  size_t len = strlen(content);
  assert(write(fd, content, len) == (ssize_t)len);
  close(fd);

  return tmk_str_from_cstr(name);
}
