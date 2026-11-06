#!/usr/bin/env bash
#
# End-to-end tests: runs a real tinymake binary on small throwaway projects.
#
#   test/e2e.sh bin/tinymake [filter]
#
# filter only runs the scenarios whose name contains it.
#
# Compiler calls go through a wrapper that records them, so the checks are about
# what was actually run rather than about the wording of the log messages.
#
# Environment:
#   CC           Real compiler (default: clang)
#   E2E_TIMEOUT  Seconds before a build counts as hung (default: 20)

if [ $# -lt 1 ] || [ ! -x "$1" ]; then
  echo "usage: $0 path/to/tinymake [filter]" >&2
  exit 2
fi

TMK=$(realpath "$1")
FILTER=${2:-}
export REAL_CC=${CC:-clang}
E2E_TIMEOUT=${E2E_TIMEOUT:-20}
ROOT=$(mktemp -d /tmp/tmk_e2e_XXXXXX)
N_PASSED=0
N_FAILED=0

# Records "compile <source>" or "link <output>" in calls.log, in the project.
# If track_jobs exists, compiling also lasts a bit longer and appends the number
# of compilers running at that moment to jobs.log
cat > "$ROOT/cc" <<'EOF'
#!/bin/sh
prev=""
source=""
output=""
for arg in "$@"; do
  [ "$prev" = "-c" ] && source=$arg
  [ "$prev" = "-o" ] && output=$arg
  prev=$arg
done
if [ -n "$source" ]; then
  echo "compile $source" >> calls.log
  if [ -e track_jobs ]; then
    : > "running.$$"
    ls running.* | wc -l >> jobs.log
    sleep 0.3
    rm -f "running.$$"
  fi
else
  echo "link $output" >> calls.log
fi
exec "$REAL_CC" "$@"
EOF
chmod +x "$ROOT/cc"

################################################################################
# Helpers. fail exits the subshell the scenario runs in.
################################################################################

fail()
{
  echo "  $*"
  echo "  --- tinymake output ---"
  sed 's/^/  | /' output.log 2>/dev/null
  exit 1
}

# Two sources and a binary: running bin/app exits with the value in util.c
new_project()
{
  mkdir src
  printf 'int util(void);\nint main(void) { return util(); }\n' > src/main.c
  set_util 0
  write_build_file "binary app: src/main.c src/util.c"
}

set_util()
{
  echo "int util(void) { return $1; }" > src/util.c
}

# Takes the target lines, an optional "cflags" and "ldflags" come from variables
write_build_file()
{
  {
    echo "cc = $ROOT/cc"
    echo "cflags = ${CFLAGS_VALUE:--O0}"
    [ -n "${LDFLAGS_VALUE:-}" ] && echo "ldflags = $LDFLAGS_VALUE"
    echo "includes = src"
    echo
    printf '%s\n' "$@"
  } > build.txt
}

# Runs tinymake, leaving the status in $STATUS, the output in output.log and
# the compiler calls in calls.log
tmk()
{
  : > calls.log
  timeout "$E2E_TIMEOUT" "$TMK" "$@" > output.log 2>&1
  STATUS=$?
  if [ "$STATUS" -eq 124 ]; then
    fail "tinymake hung: killed after ${E2E_TIMEOUT}s"
  fi
  if grep -qE 'runtime error|Sanitizer' output.log; then
    fail "tinymake triggered a sanitizer report"
  fi
}

expect_success()
{
  [ "$STATUS" -eq 0 ] || fail "expected success, got exit code $STATUS"
}

# main returns -1, anything else means something else went wrong
expect_failure()
{
  [ "$STATUS" -eq 255 ] || fail "expected exit code 255, got $STATUS"
}

# Checks the exact set of calls, in any order: expect_ran "compile a.c" "link b"
expect_ran()
{
  local expected actual
  expected=$(printf '%s\n' "$@" | sort)
  actual=$(sort calls.log)
  if [ "$expected" != "$actual" ]; then
    fail "expected calls: [$(echo $expected)], got: [$(echo $actual)]"
  fi
}

expect_nothing_ran()
{
  [ ! -s calls.log ] || fail "expected no calls, got: [$(echo $(sort calls.log))]"
}

expect_exit_value()
{
  "$1"
  local value=$?
  [ "$value" -eq "$2" ] || fail "$1 returned $value instead of $2 (stale?)"
}

expect_output()
{
  grep -qF -- "$1" output.log || fail "expected '$1' in the output"
}

expect_no_output()
{
  ! grep -qF -- "$1" output.log || fail "did not expect '$1' in the output"
}

expect_logged()
{
  grep -qF -- "$1" .tinymake_log || fail "expected '$1' in .tinymake_log"
}

expect_not_logged()
{
  ! grep -qF -- "$1" .tinymake_log || fail "did not expect '$1' in .tinymake_log"
}

expect_no_temp_files()
{
  if ls -A | grep -q '^\.tinymake_log_'; then
    fail "temporary log file left behind: $(ls -A | grep '^\.tinymake_log_')"
  fi
}

# Makes everything old, in a consistent order, so that what a scenario changes
# next is newer whatever the clock granularity
age_all()
{
  find src -type f -exec touch -d "2020-01-01 00:00:00" {} +
  find obj -type f -exec touch -d "2020-01-01 01:00:00" {} +
  find bin -type f -exec touch -d "2020-01-01 02:00:00" {} +
}

# Six extra sources, for the scenarios that need many compilations
new_big_project()
{
  new_project
  local sources="src/main.c src/util.c"
  for i in 1 2 3 4 5 6; do
    echo "int f$i(void) { return $i; }" > "src/f$i.c"
    sources="$sources src/f$i.c"
  done
  write_build_file "binary app: $sources"
}

max_jobs()
{
  sort -n jobs.log | tail -n 1
}

built_project()
{
  new_project
  tmk
  expect_success
}

################################################################################
# Nothing to do
################################################################################

first_build()
{
  new_project
  tmk
  expect_success
  expect_ran "compile src/main.c" "compile src/util.c" "link bin/app"
  expect_exit_value bin/app 0
  [ -f obj/src/main.o ] || fail "obj/src/main.o was not created"
  [ -f obj/src/main.d ] || fail "obj/src/main.d was not created"
  expect_logged "obj/src/main.o"
  expect_logged "obj/src/util.o"
  expect_logged "bin/app"
}

no_change()
{
  built_project
  tmk
  expect_success
  expect_nothing_ran
}

no_change_parallel()
{
  new_project
  tmk -j 4
  expect_success
  tmk -j 4
  expect_success
  expect_nothing_ran
}

log_format_is_stable()
{
  built_project
  cp .tinymake_log first.log
  tmk
  expect_success
  cmp -s first.log .tinymake_log || fail "the log changed on a no-change build"
}

################################################################################
# Commands changing
################################################################################

cflags_changed()
{
  built_project
  CFLAGS_VALUE="-O2" write_build_file "binary app: src/main.c src/util.c"
  tmk
  expect_success
  expect_ran "compile src/main.c" "compile src/util.c" "link bin/app"
  tmk
  expect_nothing_ran
}

# Going back to flags that were used before must not reuse the newer objects
cflags_reverted()
{
  built_project
  CFLAGS_VALUE="-O2" write_build_file "binary app: src/main.c src/util.c"
  tmk
  expect_success
  write_build_file "binary app: src/main.c src/util.c"
  tmk
  expect_success
  expect_ran "compile src/main.c" "compile src/util.c" "link bin/app"
}

ldflags_changed()
{
  built_project
  LDFLAGS_VALUE="-lm" write_build_file "binary app: src/main.c src/util.c"
  tmk
  expect_success
  expect_ran "link bin/app"
  tmk
  expect_nothing_ran
}

################################################################################
# Files changing
################################################################################

source_edited()
{
  built_project
  age_all
  set_util 3
  tmk
  expect_success
  expect_ran "compile src/util.c" "link bin/app"
  expect_exit_value bin/app 3
}

source_touched_right_after_build()
{
  built_project
  touch src/util.c
  tmk
  expect_success
  expect_ran "compile src/util.c" "link bin/app"
}

# Later overall, but with a smaller fraction of a second
newer_source_with_smaller_nsec()
{
  built_project
  age_all
  touch -d "2020-01-01 01:00:00.900000000" obj/src/util.o
  touch -d "2020-01-01 01:30:00.100000000" src/util.c
  tmk
  expect_success
  expect_ran "compile src/util.c" "link bin/app"
}

# Earlier overall, but with a bigger fraction of a second
older_source_with_bigger_nsec()
{
  built_project
  age_all
  touch -d "2019-01-01 00:00:00.900000000" src/util.c
  touch -d "2020-01-01 01:00:00.100000000" obj/src/util.o
  tmk
  expect_success
  expect_nothing_ran
}

newer_source_within_the_same_second()
{
  built_project
  touch -d "2020-01-01 10:00:00.200000000" src/util.c src/main.c obj/src/util.o
  touch -d "2020-01-01 10:00:00.100000000" obj/src/main.o
  touch -d "2020-01-01 10:00:00.900000000" bin/app
  tmk
  expect_success
  expect_ran "compile src/main.c" "link bin/app"
}

header_edited()
{
  new_project
  echo '#define VALUE 0' > src/value.h
  printf '#include "value.h"\nint util(void) { return VALUE; }\n' > src/util.c
  tmk
  expect_success
  age_all
  echo '#define VALUE 5' > src/value.h
  tmk
  expect_success
  expect_ran "compile src/util.c" "link bin/app"
  expect_exit_value bin/app 5
}

# Not an error: the old depfile still lists it, and it just triggers a rebuild
header_deleted_and_unused()
{
  new_project
  echo '#define VALUE 0' > src/value.h
  printf '#include "value.h"\nint util(void) { return VALUE; }\n' > src/util.c
  tmk
  expect_success
  rm src/value.h
  set_util 0
  age_all
  tmk
  expect_success
  expect_ran "compile src/util.c" "link bin/app"
  expect_no_output "[ERROR]"
}

header_deleted_and_still_included()
{
  new_project
  echo '#define VALUE 0' > src/value.h
  printf '#include "value.h"\nint util(void) { return VALUE; }\n' > src/util.c
  tmk
  expect_success
  rm src/value.h
  tmk
  expect_failure
}

source_deleted()
{
  built_project
  rm src/util.c
  tmk
  expect_failure
  expect_ran "compile src/util.c"
}

object_deleted()
{
  built_project
  rm obj/src/util.o
  tmk
  expect_success
  expect_ran "compile src/util.c" "link bin/app"
}

depfile_deleted()
{
  built_project
  rm obj/src/util.d
  tmk
  expect_success
  expect_ran "compile src/util.c" "link bin/app"
}

binary_deleted()
{
  built_project
  rm bin/app
  tmk
  expect_success
  expect_ran "link bin/app"
  expect_exit_value bin/app 0
}

################################################################################
# Several binaries
################################################################################

two_binaries()
{
  new_project
  write_build_file "binary app: src/main.c src/util.c" \
                   "binary app2: src/main.c src/util.c"
  tmk
  expect_success
  expect_ran "compile src/main.c" "compile src/util.c" \
             "link bin/app" "link bin/app2"
  tmk
  expect_nothing_ran
}

# The first target rebuilds the object, the second one must still relink
shared_source_edited()
{
  new_project
  write_build_file "binary app: src/main.c src/util.c" \
                   "binary app2: src/main.c src/util.c"
  tmk
  expect_success
  age_all
  set_util 7
  tmk
  expect_success
  expect_ran "compile src/util.c" "link bin/app" "link bin/app2"
  expect_exit_value bin/app 7
  expect_exit_value bin/app2 7
  tmk
  expect_nothing_ran
}

################################################################################
# Source directories
################################################################################

# Both util.c used to compile to obj/util.o, one overwriting the other
same_name_in_two_directories()
{
  mkdir -p src/b
  printf 'int util(void);\nint util_b(void);\n' > src/main.c
  printf 'int main(void) { return util() + util_b(); }\n' >> src/main.c
  set_util 3
  echo "int util_b(void) { return 4; }" > src/b/util.c
  write_build_file "binary app: src/main.c src/util.c src/b/util.c"
  tmk -j 3
  expect_success
  expect_ran "compile src/main.c" "compile src/util.c" "compile src/b/util.c" \
    "link bin/app"
  expect_exit_value bin/app 7
  expect_logged "obj/src/util.o"
  expect_logged "obj/src/b/util.o"

  tmk
  expect_success
  expect_nothing_ran

  echo "int util_b(void) { return 5; }" > src/b/util.c
  tmk
  expect_success
  expect_ran "compile src/b/util.c" "link bin/app"
  expect_exit_value bin/app 8
}

# The directory of an object cannot be created
object_directory_blocked()
{
  new_project
  mkdir obj
  touch obj/src
  tmk
  expect_failure
  expect_output "Could not create directory obj/src"
  [ ! -e bin/app ] || fail "linked without the objects"
}

################################################################################
# Parallelism
################################################################################

default_is_serial()
{
  new_big_project
  touch track_jobs
  tmk
  expect_success
  [ "$(max_jobs)" -eq 1 ] || fail "ran $(max_jobs) compilers at once, expected 1"
}

jobs_runs_in_parallel_within_the_limit()
{
  new_big_project
  touch track_jobs
  tmk -j 3
  expect_success
  [ "$(wc -l < jobs.log)" -eq 8 ] || fail "expected 8 compilations"
  [ "$(max_jobs)" -gt 1 ] || fail "-j 3 never ran two compilers at once"
  [ "$(max_jobs)" -le 3 ] || fail "ran $(max_jobs) compilers at once with -j 3"
}

################################################################################
# Failures
################################################################################

compile_failure_keeps_progress()
{
  new_project
  echo 'broken' > src/util.c
  tmk
  expect_failure
  expect_ran "compile src/main.c" "compile src/util.c"
  [ ! -e bin/app ] || fail "linked despite the compile failure"
  expect_logged "obj/src/main.o"
  expect_not_logged "obj/src/util.o"

  set_util 0
  tmk
  expect_success
  expect_ran "compile src/util.c" "link bin/app"
}

compile_failure_parallel()
{
  new_big_project
  echo 'broken' > src/f3.c
  tmk -j 4
  expect_failure
  [ ! -e bin/app ] || fail "linked despite the compile failure"

  echo 'int f3(void) { return 3; }' > src/f3.c
  tmk -j 4
  expect_success
  expect_exit_value bin/app 0
  tmk -j 4
  expect_nothing_ran
}

# Jobs already running finish, but no new ones must start
compile_failure_stops_new_jobs()
{
  new_big_project
  touch track_jobs
  echo 'broken' > src/main.c
  tmk -j 2
  expect_failure
  [ "$(wc -l < calls.log)" -lt 8 ] || fail "kept compiling after the failure"
}

link_failure_is_not_recorded()
{
  built_project
  LDFLAGS_VALUE="-lthis_library_does_not_exist" \
    write_build_file "binary app: src/main.c src/util.c"
  tmk
  expect_failure
  expect_ran "link bin/app"
  expect_not_logged "this_library_does_not_exist"

  tmk
  expect_failure
  expect_ran "link bin/app"

  write_build_file "binary app: src/main.c src/util.c"
  tmk
  expect_success
  expect_ran "link bin/app"
}

# An existing binary must not hide that the link never succeeded
link_failure_keeping_old_binary()
{
  built_project
  cp bin/app old_app
  LDFLAGS_VALUE="-lthis_library_does_not_exist" \
    write_build_file "binary app: src/main.c src/util.c"
  tmk
  expect_failure
  cp old_app bin/app
  tmk
  expect_failure
  expect_ran "link bin/app"
}

compiler_not_found()
{
  new_project
  sed -i "s|^cc = .*|cc = this_compiler_does_not_exist|" build.txt
  tmk
  expect_failure
  [ ! -e bin/app ] || fail "produced a binary without a compiler"
}

build_file_invalid_keeps_log()
{
  built_project
  cp .tinymake_log before.log
  echo 'garbage here' >> build.txt
  tmk
  expect_failure
  expect_nothing_ran
  cmp -s before.log .tinymake_log || fail "the log changed on a parse error"
}

build_file_missing()
{
  tmk
  expect_failure
  expect_nothing_ran
}

unknown_argument()
{
  new_project
  tmk --this-does-not-exist
  expect_failure
  expect_nothing_ran
}

################################################################################
# Command log
################################################################################

log_old_format()
{
  built_project
  printf 'clang -c src/main.c -o obj/src/main.o\n' > .tinymake_log
  tmk
  expect_success
  expect_ran "compile src/main.c" "compile src/util.c" "link bin/app"
  expect_output "unknown format"
  [ "$(head -n 1 .tinymake_log)" = "tinymake log v1.0" ] ||
    fail "the log was not rewritten in the current format"
  tmk
  expect_nothing_ran
}

log_deleted()
{
  built_project
  rm .tinymake_log
  tmk
  expect_success
  expect_ran "compile src/main.c" "compile src/util.c" "link bin/app"
  expect_no_output "[WARN]"
}

# Saving fails, which only costs a warning
log_cannot_be_saved()
{
  new_project
  mkdir .tinymake_log
  tmk
  expect_success
  expect_exit_value bin/app 0
  expect_output "Could not replace command log"
  expect_no_temp_files
}

no_temp_files_left()
{
  built_project
  expect_no_temp_files
  echo 'broken' > src/util.c
  tmk
  expect_failure
  expect_no_temp_files
}

################################################################################

run()
{
  local name=$1
  case "$name" in
    *"$FILTER"*) ;;
    *) return ;;
  esac

  local dir="$ROOT/$name"
  mkdir "$dir"
  printf '%-40s' "$name"

  if (cd "$dir" && "$name") > "$dir.report" 2>&1; then
    echo "ok"
    N_PASSED=$((N_PASSED + 1))
    rm -rf "$dir" "$dir.report"
  else
    echo "FAILED"
    cat "$dir.report"
    N_FAILED=$((N_FAILED + 1))
  fi
}

run first_build
run no_change
run no_change_parallel
run log_format_is_stable

run cflags_changed
run cflags_reverted
run ldflags_changed

run source_edited
run source_touched_right_after_build
run newer_source_with_smaller_nsec
run older_source_with_bigger_nsec
run newer_source_within_the_same_second
run header_edited
run header_deleted_and_unused
run header_deleted_and_still_included
run source_deleted
run object_deleted
run depfile_deleted
run binary_deleted

run two_binaries
run shared_source_edited

run same_name_in_two_directories
run object_directory_blocked

run default_is_serial
run jobs_runs_in_parallel_within_the_limit

run compile_failure_keeps_progress
run compile_failure_parallel
run compile_failure_stops_new_jobs
run link_failure_is_not_recorded
run link_failure_keeping_old_binary
run compiler_not_found
run build_file_invalid_keeps_log
run build_file_missing
run unknown_argument

run log_old_format
run log_deleted
run log_cannot_be_saved
run no_temp_files_left

if [ "$N_FAILED" -eq 0 ]; then
  rm -rf "$ROOT"
  echo "All $N_PASSED end-to-end tests passed."
  exit 0
fi

echo "$N_FAILED failed, $N_PASSED passed. Failed projects kept in $ROOT"
exit 1
