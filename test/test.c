#include <stdio.h>

#include "tinymake_string_test.h"

#define RUN(test)           \
  do                        \
  {                         \
    printf("%-32s", #test); \
    test();                 \
    printf("ok\n");         \
  } while (0)

int main(void)
{
  RUN(test_create);
  RUN(test_create_with_capacity);
  RUN(test_reserve);
  RUN(test_clear);
  RUN(test_free);
  RUN(test_from_cstr);
  RUN(test_clone);
  RUN(test_take);
  RUN(test_release);
  RUN(test_cstr);
  RUN(test_at);
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
  RUN(test_equal_nocase);

  printf("All tests passed.\n");
  return 0;
}
