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
