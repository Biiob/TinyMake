#ifndef TINYMAKE_PARSER
#define TINYMAKE_PARSER

#include "tinymake_types.h"

void create_compile_file(const char* path, compile_file_t* out);
size_t tokenize_line(char* line, char* tokens[]);
char* parse_key_value_line(const char* line);
int parse_build_file(build_context_t* ctx);

#endif
