#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>

#include "tinymake_string.h"

string_t string_create()
{
    string_t str = {
        .str = malloc(DEFAULT_STRING_SIZE),
        .len = 0,
        .capacity = DEFAULT_STRING_SIZE
    };

    return str;
}

string_t string_create_with_capacity(size_t capacity)
{
    string_t str = {
        .str = malloc(capacity),
        .len = 0,
        .capacity = capacity
    };

    return str;
}

void string_free(string_t* str)
{
    free(str->str);
    str->str = NULL;
    str->len = 0;
    str->capacity = 0;
}

string_t string_copy_from_string(const string_t* other)
{
    char* ptr = malloc(other->capacity);
    memcpy(ptr, other->str, other->len);
    string_t str = {
        .str = ptr,
        .len = other->len,
        .capacity = other->capacity
    };

    return str;
}

string_t string_copy_from_char_arr(const char* other)
{
    size_t other_len = strlen(other);
    char* ptr = malloc(other_len);
    memcpy(ptr, other, other_len);
    string_t str = {
        .str = ptr,
        .len = other_len,
        .capacity = other_len
    };

    return str;
}

char* string_get(string_t* str)
{
    str->str[str->len] = '\0';
    return str->str;
}

string_t* string_append_from_string(const string_t* src, string_t* dst)
{
    size_t needed_capacity = dst->len + src->len + 1;

    if(needed_capacity > dst->capacity)
    {
        char* tmp = realloc(dst->str, needed_capacity);
        if(tmp == NULL)
        {
            return NULL;
        }
        dst->str = tmp;
        dst->capacity = needed_capacity;
    }

    memcpy(dst->str + dst->len, src->str, src->len);
    dst->len = dst->len + src->len;
    return dst;
}

string_t* string_append_from_char_arr(const char* src, string_t* dst)
{
    size_t src_len = strlen(src);
    size_t needed_capacity = dst->len + src_len + 1;

    if(needed_capacity > dst->capacity)
    {
        char* tmp = realloc(dst->str, needed_capacity);
        if(tmp == NULL)
        {
            return NULL;
        }
        dst->str = tmp;
        dst->capacity = needed_capacity;
    }

    memcpy(dst->str + dst->len, src, src_len);
    dst->len = dst->len + src_len;
    return dst;
}

string_t* string_format(string_t* dst, const char* fmt, ...)
{
    va_list ap, ap2;
    va_start(ap, fmt);
    va_copy(ap2, ap);

    size_t needed_capacity = vsnprintf(NULL, 0, fmt, ap) + 1;
    if(needed_capacity > dst->capacity)
    {
        char* tmp = realloc(dst->str, needed_capacity);
        if(tmp == NULL)
        {
            return NULL;
        }
        dst->str = tmp;
        dst->capacity = needed_capacity;
    }
    vsnprintf(dst->str, dst->capacity, fmt, ap2);
    dst->len = needed_capacity - 1;
    va_end(ap);
    va_end(ap2);

    return dst;
}

string_t* string_trim(string_t* str)
{
    trim_whitespaces(str->str);
    return str;
}

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
