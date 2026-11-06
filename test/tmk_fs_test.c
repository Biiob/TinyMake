#include "tmk_fs.h"

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "tmk_fs_test.h"
#include "tmk_str.h"

static char g_previous_dir[PATH_MAX];
static char g_sandbox_dir[32];

/* Directories are created relative to the current directory, so every test
 * runs in an empty temporary one.
 */
static void enter_sandbox(void)
{
  assert(getcwd(g_previous_dir, sizeof g_previous_dir) != NULL);
  strcpy(g_sandbox_dir, "/tmp/tmk_fs_test_XXXXXX");
  assert(mkdtemp(g_sandbox_dir) != NULL);
  assert(chdir(g_sandbox_dir) == 0);
}

/* Fails if the test left anything behind it did not remove. */
static void leave_sandbox(void)
{
  assert(chdir(g_previous_dir) == 0);
  assert(rmdir(g_sandbox_dir) == 0);
}

static bool create_dir(const char* path)
{
  tmk_str p = tmk_str_from_cstr(path);
  bool rc = tmk_fs_create_dir(&p);
  tmk_str_free(&p);

  return rc;
}

static bool is_dir(const char* path)
{
  struct stat s;
  return stat(path, &s) == 0 && S_ISDIR(s.st_mode);
}

static void write_file(const char* path)
{
  FILE* f = fopen(path, "w");
  assert(f != NULL);
  fclose(f);
}

void test_fs_create_dir(void)
{
  enter_sandbox();

  assert(create_dir("a"));
  assert(is_dir("a"));

  assert(rmdir("a") == 0);
  leave_sandbox();
}

void test_fs_create_dir_nested(void)
{
  enter_sandbox();

  assert(create_dir("a/b/c"));
  assert(is_dir("a/b/c"));

  assert(rmdir("a/b/c") == 0);
  assert(rmdir("a/b") == 0);
  assert(rmdir("a") == 0);
  leave_sandbox();
}

void test_fs_create_dir_existing(void)
{
  /* Existing directories, complete or partial, are not an error. */
  enter_sandbox();

  assert(create_dir("a"));
  assert(create_dir("a"));

  assert(create_dir("a/b/c"));
  assert(create_dir("a/b/c"));
  assert(create_dir("a/b"));
  assert(create_dir("a/b/d"));
  assert(is_dir("a/b/c"));
  assert(is_dir("a/b/d"));

  assert(rmdir("a/b/c") == 0);
  assert(rmdir("a/b/d") == 0);
  assert(rmdir("a/b") == 0);
  assert(rmdir("a") == 0);
  leave_sandbox();
}

void test_fs_create_dir_absolute(void)
{
  enter_sandbox();

  char path[PATH_MAX];
  snprintf(path, sizeof path, "%s/a/b", g_sandbox_dir);
  assert(create_dir(path));
  assert(is_dir("a/b"));

  assert(rmdir("a/b") == 0);
  assert(rmdir("a") == 0);
  leave_sandbox();
}

void test_fs_create_dir_empty(void)
{
  /* The directory of "main.c" is "": there is nothing to create. */
  enter_sandbox();

  assert(create_dir(""));

  leave_sandbox();
}

void test_fs_create_dir_file_in_the_way(void)
{
  enter_sandbox();
  write_file("f");

  /* A file is not a directory, even though mkdir reports EEXIST. */
  assert(!create_dir("f"));
  assert(!create_dir("f/a"));
  assert(!create_dir("f/a/b"));

  assert(unlink("f") == 0);
  leave_sandbox();
}

void test_fs_create_dir_not_allowed(void)
{
  /* root ignores permissions, so the test means nothing there. */
  if (geteuid() == 0)
  {
    return;
  }

  enter_sandbox();
  assert(mkdir("locked", 0555) == 0);

  assert(!create_dir("locked/a"));
  assert(!create_dir("locked/a/b"));
  assert(!is_dir("locked/a"));

  assert(rmdir("locked") == 0);
  leave_sandbox();
}

void test_fs_init_work_dirs(void)
{
  enter_sandbox();

  assert(tmk_fs_init_work_dirs());
  assert(is_dir("obj"));
  assert(is_dir("bin"));

  /* Every build after the first finds them already there. */
  assert(tmk_fs_init_work_dirs());

  assert(rmdir("obj") == 0);
  assert(rmdir("bin") == 0);
  leave_sandbox();
}

static struct stat at(time_t sec, long nsec)
{
  struct stat s = {0};
  s.st_mtim.tv_sec = sec;
  s.st_mtim.tv_nsec = nsec;

  return s;
}

void test_fs_is_newer(void)
{
  struct stat a = at(100, 500);
  struct stat b = at(100, 500);
  assert(!tmk_fs_is_newer(&a, &b));

  /* Seconds decide first, whatever the fraction of a second. */
  a = at(101, 0);
  b = at(100, 999999999);
  assert(tmk_fs_is_newer(&a, &b));
  assert(!tmk_fs_is_newer(&b, &a));

  /* Within the same second, nanoseconds decide. */
  a = at(100, 501);
  b = at(100, 500);
  assert(tmk_fs_is_newer(&a, &b));
  assert(!tmk_fs_is_newer(&b, &a));
}
