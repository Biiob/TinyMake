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
  RUN(test_free);
  RUN(test_copy_from_char_array);
  RUN(test_copy_from_string);
  RUN(test_assign);
  RUN(test_get);
  RUN(test_at);
  RUN(test_append_from_char_array);
  RUN(test_append_from_string);
  RUN(test_append_grows);
  RUN(test_format);
  RUN(test_format_grows);
  RUN(test_trim);

  printf("All tests passed.\n");
  return 0;
}
