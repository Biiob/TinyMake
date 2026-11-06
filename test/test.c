#include <stdio.h>

#include "tmk_cmdlog_test.h"
#include "tmk_depfile_test.h"
#include "tmk_log.h"
#include "tmk_parser_test.h"
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
  // Expected errors from the tests would drown the results
  log_set_sink(NULL);

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
  RUN(test_find_next_char);
  RUN(test_rfind_char);
  RUN(test_read_line);

  RUN(test_str_array_zeroed);
  RUN(test_str_array_push);
  RUN(test_str_array_push_grows);
  RUN(test_str_array_take);
  RUN(test_tokenize);

  RUN(test_depfile_single_line);
  RUN(test_depfile_continuation_lines);
  RUN(test_depfile_no_trailing_newline);
  RUN(test_depfile_extra_whitespace);
  RUN(test_depfile_growth);
  RUN(test_depfile_no_prerequisites);
  RUN(test_depfile_empty);
  RUN(test_depfile_missing_file);

  RUN(test_parser_valid);
  RUN(test_parser_layout);
  RUN(test_parser_empty_list);
  RUN(test_parser_invalid);
  RUN(test_parser_missing_file);

  RUN(test_cmdlog_load_missing_file);
  RUN(test_cmdlog_load_empty_file);
  RUN(test_cmdlog_load_header_only);
  RUN(test_cmdlog_load_entries);
  RUN(test_cmdlog_load_no_trailing_newline);
  RUN(test_cmdlog_load_skipped_lines);
  RUN(test_cmdlog_load_tab_in_command);
  RUN(test_cmdlog_load_wrong_header);
  RUN(test_cmdlog_find);
  RUN(test_cmdlog_record_new_output);
  RUN(test_cmdlog_record_existing_output);
  RUN(test_cmdlog_record_copies);
  RUN(test_cmdlog_save_format);
  RUN(test_cmdlog_save_empty);
  RUN(test_cmdlog_save_uses_path);
  RUN(test_cmdlog_save_replaces_existing);
  RUN(test_cmdlog_save_invalid_path);
  RUN(test_cmdlog_round_trip);
  RUN(test_cmdlog_round_trip_is_stable);
  RUN(test_cmdlog_free);

  printf("All tests passed.\n");
  return 0;
}
