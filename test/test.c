#include <stdio.h>

#include "tmk_depfile_test.h"
#include "tmk_str_array_test.h"
#include "tmk_str_test.h"

#define RUN(test)           \
  do                        \
  {                         \
    printf("%-40s", #test); \
    test();                 \
    printf("ok\n");         \
  } while (0)

int main(void)
{
  RUN(test_create);
  RUN(test_create_with_capacity);
  RUN(test_grow_min_capacity);
  RUN(test_reserve);
  RUN(test_clear);
  RUN(test_free);
  RUN(test_from_buf);
  RUN(test_from_cstr);
  RUN(test_clone);
  RUN(test_substr);
  RUN(test_take);
  RUN(test_release);
  RUN(test_cstr);
  RUN(test_set);
  RUN(test_append_n);
  RUN(test_append_cstr);
  RUN(test_append);
  RUN(test_append_grows);
  RUN(test_push);
  RUN(test_format);
  RUN(test_format_grows);
  RUN(test_append_format);
  RUN(test_trim);
  RUN(test_equal);
  RUN(test_equal_cstr);
  RUN(test_equal_nocase);
  RUN(test_find_char);
  RUN(test_rfind_char);
  RUN(test_read_line);

  RUN(test_str_array_zeroed);
  RUN(test_str_array_push);
  RUN(test_str_array_push_grows);
  RUN(test_tokenize);

  RUN(test_depfile_single_line);
  RUN(test_depfile_continuation_lines);
  RUN(test_depfile_no_trailing_newline);
  RUN(test_depfile_extra_whitespace);
  RUN(test_depfile_growth);
  RUN(test_depfile_no_prerequisites);
  RUN(test_depfile_empty);
  RUN(test_depfile_missing_file);

  printf("All tests passed.\n");
  return 0;
}
