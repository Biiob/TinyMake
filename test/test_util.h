#ifndef TEST_UTIL_H
#define TEST_UTIL_H

#include "tmk_str.h"

// Writes content to a new file in /tmp and returns its path
// The caller must unlink the file, then free the path
tmk_str test_write_temp_file(const char* content);

// Makes an empty temporary directory the current one, so that a test can
// create files without touching the project. Returns its absolute path
const char* test_enter_sandbox(void);

// Goes back to the previous directory. Fails if the sandbox is not empty
void test_leave_sandbox(void);

#endif
