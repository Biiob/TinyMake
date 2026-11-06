#include <stdio.h>

#include "tmk_cmdlog_test.h"
#include "tmk_depfile_test.h"
#include "tmk_fs_test.h"
#include "tmk_log.h"
#include "tmk_parser_test.h"
#include "tmk_path_test.h"
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
  tmk_log_set_sink(NULL);

  RUN(test_str_create);
  RUN(test_str_create_with_capacity);
  RUN(test_str_grow_min_capacity);
  RUN(test_str_reserve);
  RUN(test_str_clear);
  RUN(test_str_free);
  RUN(test_str_from_buf);
  RUN(test_str_from_cstr);
  RUN(test_str_clone);
  RUN(test_str_substr);
  RUN(test_str_take);
  RUN(test_str_release);
  RUN(test_str_cstr);
  RUN(test_str_append_n);
  RUN(test_str_append_cstr);
  RUN(test_str_append);
  RUN(test_str_append_grows);
  RUN(test_str_push);
  RUN(test_str_format);
  RUN(test_str_format_grows);
  RUN(test_str_append_format);
  RUN(test_str_trim);
  RUN(test_str_equal);
  RUN(test_str_equal_cstr);
  RUN(test_str_equal_nocase);
  RUN(test_str_find_char);
  RUN(test_str_find_next_char);
  RUN(test_str_rfind_char);
  RUN(test_str_read_line);
  RUN(test_str_zeroed);

  RUN(test_str_array_zeroed);
  RUN(test_str_array_push);
  RUN(test_str_array_push_grows);
  RUN(test_str_array_take);
  RUN(test_str_tokenize);

  RUN(test_depfile_single_line);
  RUN(test_depfile_continuation_lines);
  RUN(test_depfile_no_trailing_newline);
  RUN(test_depfile_extra_whitespace);
  RUN(test_depfile_no_prerequisites);
  RUN(test_depfile_empty);
  RUN(test_depfile_missing_file);

  RUN(test_parser_valid);
  RUN(test_parser_layout);
  RUN(test_parser_empty_list);
  RUN(test_parser_output_paths);
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
  RUN(test_cmdlog_record_existing_output);
  RUN(test_cmdlog_record_copies);
  RUN(test_cmdlog_save_format);
  RUN(test_cmdlog_save_empty);
  RUN(test_cmdlog_save_replaces_existing);
  RUN(test_cmdlog_save_invalid_path);
  RUN(test_cmdlog_round_trip);
  RUN(test_cmdlog_free);

  RUN(test_path_get_dir);
  RUN(test_path_without_ext);

  RUN(test_fs_create_dir_nested);
  RUN(test_fs_create_dir_existing);
  RUN(test_fs_create_dir_absolute);
  RUN(test_fs_create_dir_empty);
  RUN(test_fs_create_dir_file_in_the_way);
  RUN(test_fs_create_dir_not_allowed);
  RUN(test_fs_is_newer);

  printf("All tests passed.\n");
  return 0;
}
