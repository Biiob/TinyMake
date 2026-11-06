#ifndef TMK_CMDLOG_TEST_H
#define TMK_CMDLOG_TEST_H

void test_cmdlog_load_missing_file(void);
void test_cmdlog_load_empty_file(void);
void test_cmdlog_load_header_only(void);
void test_cmdlog_load_entries(void);
void test_cmdlog_load_no_trailing_newline(void);
void test_cmdlog_load_skipped_lines(void);
void test_cmdlog_load_tab_in_command(void);
void test_cmdlog_load_wrong_header(void);
void test_cmdlog_find(void);
void test_cmdlog_record_existing_output(void);
void test_cmdlog_record_copies(void);
void test_cmdlog_save_format(void);
void test_cmdlog_save_empty(void);
void test_cmdlog_save_replaces_existing(void);
void test_cmdlog_save_invalid_path(void);
void test_cmdlog_round_trip(void);
void test_cmdlog_free(void);

#endif
