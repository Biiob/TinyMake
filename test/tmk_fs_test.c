#include "tmk_fs.h"

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "test_util.h"
#include "tmk_fs_test.h"
#include "tmk_str.h"

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

void test_fs_create_dir_nested(void)
{
  test_enter_sandbox();

  assert(create_dir("a/b/c"));
  assert(is_dir("a/b/c"));

  assert(rmdir("a/b/c") == 0);
  assert(rmdir("a/b") == 0);
  assert(rmdir("a") == 0);
  test_leave_sandbox();
}

void test_fs_create_dir_existing(void)
{
  // Existing directories, complete or partial, are not an error
  test_enter_sandbox();

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
  test_leave_sandbox();
}

void test_fs_create_dir_absolute(void)
{
  const char* sandbox = test_enter_sandbox();

  char path[PATH_MAX];
  snprintf(path, sizeof path, "%s/a/b", sandbox);
  assert(create_dir(path));
  assert(is_dir("a/b"));

  assert(rmdir("a/b") == 0);
  assert(rmdir("a") == 0);
  test_leave_sandbox();
}

void test_fs_create_dir_empty(void)
{
  // The directory of "main.c" is "": there is nothing to create
  test_enter_sandbox();

  assert(create_dir(""));

  test_leave_sandbox();
}

void test_fs_create_dir_file_in_the_way(void)
{
  test_enter_sandbox();
  write_file("f");

  // A file is not a directory, even though mkdir reports EEXIST
  assert(!create_dir("f"));
  assert(!create_dir("f/a"));
  assert(!create_dir("f/a/b"));

  assert(unlink("f") == 0);
  test_leave_sandbox();
}

void test_fs_create_dir_not_allowed(void)
{
  // root ignores permissions, so the test means nothing there
  if (geteuid() == 0)
  {
    return;
  }

  test_enter_sandbox();
  assert(mkdir("locked", 0555) == 0);

  assert(!create_dir("locked/a"));
  assert(!create_dir("locked/a/b"));
  assert(!is_dir("locked/a"));

  assert(rmdir("locked") == 0);
  test_leave_sandbox();
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

  // Seconds decide first, whatever the fraction of a second
  a = at(101, 0);
  b = at(100, 999999999);
  assert(tmk_fs_is_newer(&a, &b));
  assert(!tmk_fs_is_newer(&b, &a));

  // Within the same second, nanoseconds decide
  a = at(100, 501);
  b = at(100, 500);
  assert(tmk_fs_is_newer(&a, &b));
  assert(!tmk_fs_is_newer(&b, &a));
}
