#ifndef TMK_STR_TEST_H
#define TMK_STR_TEST_H

void test_create(void);
void test_create_with_capacity(void);
void test_reserve(void);
void test_clear(void);
void test_free(void);
void test_from_buf(void);
void test_from_cstr(void);
void test_clone(void);
void test_substr(void);
void test_take(void);
void test_release(void);
void test_cstr(void);
void test_set(void);
void test_append_n(void);
void test_append_cstr(void);
void test_append(void);
void test_append_grows(void);
void test_push(void);
void test_format(void);
void test_format_grows(void);
void test_append_format(void);
void test_trim(void);
void test_equal(void);
void test_equal_cstr(void);
void test_equal_nocase(void);
void test_tokenize(void);
void test_find_char(void);
void test_rfind_char(void);
void test_read_line(void);

#endif
