#include "test_util.h"

#include <assert.h>
#include <limits.h>
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

static char g_previous_dir[PATH_MAX];
static char g_sandbox_dir[32];

const char* test_enter_sandbox(void)
{
  assert(getcwd(g_previous_dir, sizeof g_previous_dir) != NULL);
  strcpy(g_sandbox_dir, "/tmp/tmk_test_XXXXXX");
  assert(mkdtemp(g_sandbox_dir) != NULL);
  assert(chdir(g_sandbox_dir) == 0);

  return g_sandbox_dir;
}

void test_leave_sandbox(void)
{
  assert(chdir(g_previous_dir) == 0);
  assert(rmdir(g_sandbox_dir) == 0);
}
