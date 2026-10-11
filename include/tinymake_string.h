#ifndef TINYMAKE_STRING
#define TINYMAKE_STRING

#include <stdlib.h>

char* trim_whitespaces(char* src);
size_t tokenize_line(char* line, char* tokens[]);

#endif
