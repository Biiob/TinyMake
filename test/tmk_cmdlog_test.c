#include "tmk_cmdlog.h"

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "test_util.h"
#include "tmk_cmdlog_test.h"

#define HEADER "tinymake log v1.0\n"

#define LOG_NAME "cmdlog"

static char g_previous_dir[PATH_MAX];
static char g_sandbox_dir[32];

/* Saving creates files next to the log, and must never touch the real
 * .tinymake_log of the project, so all saves happen in a temporary directory.
 */
static void enter_sandbox(void)
{
  assert(getcwd(g_previous_dir, sizeof g_previous_dir) != NULL);
  strcpy(g_sandbox_dir, "/tmp/tmk_cmdlog_test_XXXXXX");
  assert(mkdtemp(g_sandbox_dir) != NULL);
  assert(chdir(g_sandbox_dir) == 0);
}

/* Fails if the test left anything but LOG_NAME behind, like a temporary file.
 */
static void leave_sandbox(void)
{
  unlink(LOG_NAME);
  assert(chdir(g_previous_dir) == 0);
  assert(rmdir(g_sandbox_dir) == 0);
}

static tmk_str read_file(const char* path)
{
  FILE* f = fopen(path, "rb");
  assert(f != NULL);

  char buf[4096];
  size_t n = fread(buf, sizeof(char), sizeof buf, f);
  assert(feof(f));
  fclose(f);

  return tmk_str_from_buf(buf, n);
}

static void write_file(const char* path, const char* content)
{
  FILE* f = fopen(path, "wb");
  assert(f != NULL);
  assert(fputs(content, f) >= 0);
  fclose(f);
}

static void check_file(const char* path, const char* expected)
{
  tmk_str content = read_file(path);
  assert(content.len == strlen(expected));
  assert(memcmp(tmk_str_cstr(&content), expected, content.len) == 0);
  tmk_str_free(&content);
}

/* Loads content into a zeroed log. */
static void load(const char* content, tmk_cmdlog* log)
{
  tmk_str path = test_write_temp_file(content);
  *log = (tmk_cmdlog){0};
  tmk_cmdlog_load(log, &path);
  unlink(tmk_str_cstr(&path));
  tmk_str_free(&path);
}

static void check_empty(const tmk_cmdlog* log)
{
  assert(log->outputs.len == 0);
  assert(log->commands.len == 0);
}

static void check_entry(const tmk_cmdlog* log, const char* output,
                        const char* command)
{
  tmk_str key = tmk_str_from_cstr(output);
  const tmk_str* found = tmk_cmdlog_find(log, &key);
  assert(found != NULL);
  assert(tmk_str_equal_cstr(found, command));
  tmk_str_free(&key);
}

static void check_not_found(const tmk_cmdlog* log, const char* output)
{
  tmk_str key = tmk_str_from_cstr(output);
  assert(tmk_cmdlog_find(log, &key) == NULL);
  tmk_str_free(&key);
}

static void record(tmk_cmdlog* log, const char* output, const char* command)
{
  tmk_str o = tmk_str_from_cstr(output);
  tmk_str c = tmk_str_from_cstr(command);
  tmk_cmdlog_record(log, &o, &c);
  tmk_str_free(&o);
  tmk_str_free(&c);
}

static int save(const tmk_cmdlog* log, const char* path)
{
  tmk_str p = tmk_str_from_cstr(path);
  int rc = tmk_cmdlog_save(log, &p);
  tmk_str_free(&p);

  return rc;
}

void test_cmdlog_load_missing_file(void)
{
  /* The first build has no log: everything gets rebuilt. */
  tmk_str path = tmk_str_from_cstr("/tmp/tmk_cmdlog_test_does_not_exist");
  tmk_cmdlog log = {0};
  tmk_cmdlog_load(&log, &path);
  check_empty(&log);

  tmk_cmdlog_free(&log);
  tmk_str_free(&path);
}

void test_cmdlog_load_empty_file(void)
{
  tmk_cmdlog log;
  load("", &log);
  check_empty(&log);

  tmk_cmdlog_free(&log);
}

void test_cmdlog_load_header_only(void)
{
  tmk_cmdlog log;
  load(HEADER, &log);
  check_empty(&log);

  tmk_cmdlog_free(&log);
}

void test_cmdlog_load_entries(void)
{
  tmk_cmdlog log;
  load(HEADER
       "obj/a.o\tclang -c src/a.c -o obj/a.o\n"
       "bin/app\tclang -o bin/app obj/a.o\n",
       &log);

  assert(log.outputs.len == 2);
  assert(log.commands.len == 2);
  /* The line terminator is not part of the command. */
  check_entry(&log, "obj/a.o", "clang -c src/a.c -o obj/a.o");
  check_entry(&log, "bin/app", "clang -o bin/app obj/a.o");

  tmk_cmdlog_free(&log);
}

void test_cmdlog_load_no_trailing_newline(void)
{
  tmk_cmdlog log;
  load(HEADER "obj/a.o\tclang -c src/a.c", &log);

  assert(log.outputs.len == 1);
  check_entry(&log, "obj/a.o", "clang -c src/a.c");

  tmk_cmdlog_free(&log);
}

void test_cmdlog_load_skipped_lines(void)
{
  /* Blank lines, comments and lines without a tab are not entries. */
  tmk_cmdlog log;
  load(HEADER
       "\n"
       "# obj/comment.o\tclang -c src/comment.c\n"
       "obj/a.o\tclang -c src/a.c\n"
       "no separator on this line\n"
       "\n"
       "obj/b.o\tclang -c src/b.c\n",
       &log);

  assert(log.outputs.len == 2);
  assert(log.commands.len == 2);
  check_entry(&log, "obj/a.o", "clang -c src/a.c");
  check_entry(&log, "obj/b.o", "clang -c src/b.c");
  check_not_found(&log, "# obj/comment.o");

  tmk_cmdlog_free(&log);
}

void test_cmdlog_load_tab_in_command(void)
{
  /* Only the first tab is a separator. */
  tmk_cmdlog log;
  load(HEADER "obj/a.o\tclang\t-c\tsrc/a.c\n", &log);

  assert(log.outputs.len == 1);
  check_entry(&log, "obj/a.o", "clang\t-c\tsrc/a.c");

  tmk_cmdlog_free(&log);
}

void test_cmdlog_load_wrong_header(void)
{
  /* An unknown format is ignored: everything gets rebuilt. */
  const char* contents[] = {
      "tinymake log v0.1\nobj/a.o\tclang -c src/a.c\n",
      "tinymake log v1.0 and more\nobj/a.o\tclang -c src/a.c\n",
      /* The format used before the header was introduced. */
      "clang -c src/a.c -o obj/a.o\n",
      /* The header is only valid on the first line. */
      ("obj/a.o\tclang -c src/a.c\n" HEADER),
  };

  for (size_t i = 0; i < sizeof contents / sizeof *contents; ++i)
  {
    tmk_cmdlog log;
    load(contents[i], &log);
    check_empty(&log);
    tmk_cmdlog_free(&log);
  }
}

void test_cmdlog_find(void)
{
  tmk_cmdlog log = {0};
  check_not_found(&log, "obj/a.o");

  record(&log, "obj/a.o", "first");
  record(&log, "obj/b.o", "second");

  check_entry(&log, "obj/a.o", "first");
  check_entry(&log, "obj/b.o", "second");

  /* The whole output must match. */
  check_not_found(&log, "obj/c.o");
  check_not_found(&log, "obj/a");
  check_not_found(&log, "obj/a.o.d");
  check_not_found(&log, "");

  tmk_cmdlog_free(&log);
}

void test_cmdlog_record_new_output(void)
{
  tmk_cmdlog log = {0};

  record(&log, "obj/a.o", "first");
  assert(log.outputs.len == 1);
  assert(log.commands.len == 1);
  check_entry(&log, "obj/a.o", "first");

  record(&log, "obj/b.o", "second");
  assert(log.outputs.len == 2);
  assert(log.commands.len == 2);
  check_entry(&log, "obj/a.o", "first");
  check_entry(&log, "obj/b.o", "second");

  tmk_cmdlog_free(&log);
}

void test_cmdlog_record_existing_output(void)
{
  tmk_cmdlog log = {0};
  record(&log, "obj/a.o", "first");
  record(&log, "obj/b.o", "second");

  /* Longer than the previous command, to force the string to grow. */
  record(&log, "obj/a.o", "first but with a much longer command than before");
  assert(log.outputs.len == 2);
  assert(log.commands.len == 2);
  check_entry(&log, "obj/a.o",
              "first but with a much longer command than before");
  check_entry(&log, "obj/b.o", "second");

  record(&log, "obj/a.o", "short");
  assert(log.outputs.len == 2);
  check_entry(&log, "obj/a.o", "short");

  tmk_cmdlog_free(&log);
}

void test_cmdlog_record_copies(void)
{
  /* The log owns its strings: the caller's can change or go away. */
  tmk_cmdlog log = {0};
  tmk_str output = tmk_str_from_cstr("obj/a.o");
  tmk_str command = tmk_str_from_cstr("first");

  tmk_cmdlog_record(&log, &output, &command);
  assert(log.outputs.items[0].str != output.str);
  assert(log.commands.items[0].str != command.str);

  tmk_str_format(&command, "changed");
  check_entry(&log, "obj/a.o", "first");

  tmk_str_free(&output);
  tmk_str_free(&command);
  check_entry(&log, "obj/a.o", "first");

  tmk_cmdlog_free(&log);
}

void test_cmdlog_save_format(void)
{
  enter_sandbox();

  tmk_cmdlog log = {0};
  record(&log, "obj/a.o", "clang -c src/a.c");
  record(&log, "bin/app", "clang -o bin/app obj/a.o");

  assert(save(&log, LOG_NAME) == 0);
  check_file(LOG_NAME, HEADER
             "obj/a.o\tclang -c src/a.c\n"
             "bin/app\tclang -o bin/app obj/a.o\n");

  tmk_cmdlog_free(&log);
  leave_sandbox();
}

void test_cmdlog_save_empty(void)
{
  enter_sandbox();

  tmk_cmdlog log = {0};
  assert(save(&log, LOG_NAME) == 0);
  check_file(LOG_NAME, HEADER);

  tmk_cmdlog_free(&log);
  leave_sandbox();
}

void test_cmdlog_save_uses_path(void)
{
  enter_sandbox();

  tmk_cmdlog log = {0};
  record(&log, "obj/a.o", "first");

  assert(save(&log, LOG_NAME) == 0);
  assert(access(LOG_NAME, F_OK) == 0);
  assert(access(".tinymake_log", F_OK) != 0);

  tmk_cmdlog_free(&log);
  leave_sandbox();
}

void test_cmdlog_save_replaces_existing(void)
{
  enter_sandbox();

  write_file(LOG_NAME, HEADER "obj/old.o\told command, longer than the new\n");

  tmk_cmdlog log = {0};
  record(&log, "obj/a.o", "new");
  assert(save(&log, LOG_NAME) == 0);
  check_file(LOG_NAME, HEADER "obj/a.o\tnew\n");

  tmk_cmdlog_free(&log);
  leave_sandbox();
}

void test_cmdlog_save_invalid_path(void)
{
  /* The temporary file must not be left behind: see leave_sandbox. */
  enter_sandbox();

  tmk_cmdlog log = {0};
  record(&log, "obj/a.o", "first");
  assert(save(&log, "does_not_exist/" LOG_NAME) == -1);

  tmk_cmdlog_free(&log);
  leave_sandbox();
}

void test_cmdlog_round_trip(void)
{
  enter_sandbox();

  tmk_cmdlog log = {0};
  record(&log, "obj/a.o", "clang -c src/a.c -o obj/a.o");
  record(&log, "obj/b.o", "clang\t-c\tsrc/b.c");
  record(&log, "bin/app", "clang -o bin/app obj/a.o obj/b.o");
  assert(save(&log, LOG_NAME) == 0);
  tmk_cmdlog_free(&log);

  tmk_str path = tmk_str_from_cstr(LOG_NAME);
  tmk_cmdlog loaded = {0};
  tmk_cmdlog_load(&loaded, &path);

  assert(loaded.outputs.len == 3);
  assert(loaded.commands.len == 3);
  check_entry(&loaded, "obj/a.o", "clang -c src/a.c -o obj/a.o");
  check_entry(&loaded, "obj/b.o", "clang\t-c\tsrc/b.c");
  check_entry(&loaded, "bin/app", "clang -o bin/app obj/a.o obj/b.o");

  tmk_cmdlog_free(&loaded);
  tmk_str_free(&path);
  leave_sandbox();
}

void test_cmdlog_round_trip_is_stable(void)
{
  /* What a real build does: each run loads the log of the previous one,
   * updates some of the entries, and saves it back. */
  enter_sandbox();

  tmk_str path = tmk_str_from_cstr(LOG_NAME);

  tmk_cmdlog log = {0};
  record(&log, "obj/a.o", "a v1");
  record(&log, "obj/b.o", "b v1");
  assert(save(&log, LOG_NAME) == 0);
  tmk_cmdlog_free(&log);

  /* Nothing changed: the file comes out identical. */
  log = (tmk_cmdlog){0};
  tmk_cmdlog_load(&log, &path);
  assert(save(&log, LOG_NAME) == 0);
  check_file(LOG_NAME, HEADER "obj/a.o\ta v1\nobj/b.o\tb v1\n");
  tmk_cmdlog_free(&log);

  /* One command changed and one output is new. */
  log = (tmk_cmdlog){0};
  tmk_cmdlog_load(&log, &path);
  record(&log, "obj/a.o", "a v2");
  record(&log, "bin/app", "link");
  assert(save(&log, LOG_NAME) == 0);
  check_file(LOG_NAME, HEADER "obj/a.o\ta v2\nobj/b.o\tb v1\nbin/app\tlink\n");
  tmk_cmdlog_free(&log);

  tmk_str_free(&path);
  leave_sandbox();
}

void test_cmdlog_free(void)
{
  /* A zeroed log is empty and can be freed, even twice. */
  tmk_cmdlog log = {0};
  tmk_cmdlog_free(&log);
  tmk_cmdlog_free(&log);

  record(&log, "obj/a.o", "first");
  tmk_cmdlog_free(&log);
  assert(log.outputs.items == NULL);
  assert(log.commands.items == NULL);
  check_empty(&log);
  tmk_cmdlog_free(&log);
}
