#include <string.h>
#include <ctype.h>

#include "tinymake_string.h"

char* trim_whitespaces(char* src)
{
    char* s = src;
    while(*s && isspace((unsigned char) *s))
    {
        s++;
    }

    if(*s == '\0')
    {
        return s;
    }

    char* e = s + strlen(s) - 1;
    while(e > s && isspace((unsigned char) *e))
    {
        e--;
    }

    e[1] = '\0';

    return s;
}

size_t tokenize_line(char* line, char* tokens[])
{
    char* save_buffer;
    size_t token_count = 0;
    for(char* token = strtok_r(line, " ", &save_buffer); token != NULL; token = strtok_r(NULL, " ", &save_buffer), ++token_count)
    {
        tokens[token_count] = token;
    }

    return token_count;
}
