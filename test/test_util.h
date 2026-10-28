#ifndef TEST_UTIL_H
#define TEST_UTIL_H

#include "tmk_str.h"

// Writes content to a new file in /tmp and returns its path
// The caller must unlink the file, then free the path
tmk_str test_write_temp_file(const char* content);

#endif
