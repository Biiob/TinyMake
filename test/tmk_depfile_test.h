#ifndef TMK_DEPFILE_TEST_H
#define TMK_DEPFILE_TEST_H

void test_depfile_single_line(void);
void test_depfile_continuation_lines(void);
void test_depfile_no_trailing_newline(void);
void test_depfile_extra_whitespace(void);
void test_depfile_no_prerequisites(void);
void test_depfile_empty(void);
void test_depfile_missing_file(void);

#endif
