#ifndef TMK_STR_TEST_H
#define TMK_STR_TEST_H

void test_str_create(void);
void test_str_grow_min_capacity(void);
void test_str_create_with_capacity(void);
void test_str_reserve(void);
void test_str_clear(void);
void test_str_free(void);
void test_str_from_buf(void);
void test_str_from_cstr(void);
void test_str_clone(void);
void test_str_substr(void);
void test_str_take(void);
void test_str_release(void);
void test_str_cstr(void);
void test_str_append_n(void);
void test_str_append_cstr(void);
void test_str_append(void);
void test_str_append_grows(void);
void test_str_push(void);
void test_str_format(void);
void test_str_format_grows(void);
void test_str_append_format(void);
void test_str_trim(void);
void test_str_equal(void);
void test_str_equal_cstr(void);
void test_str_equal_nocase(void);
void test_str_find_char(void);
void test_str_find_next_char(void);
void test_str_rfind_char(void);
void test_str_read_line(void);
void test_str_zeroed(void);

#endif
