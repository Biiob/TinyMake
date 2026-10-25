#ifndef TMK_PATH_H
#define TMK_PATH_H

void get_file_path(const char* full_path, char* out);
void get_file_name(const char* full_path, char* out);
void get_file_name_without_ext(const char* full_path, char* out);
#endif
