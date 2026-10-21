#ifndef TINYMAKE_STRING
#define TINYMAKE_STRING

#include <stdlib.h>

#define DEFAULT_STRING_SIZE 256
typedef struct
{
    char* str;
    size_t len;
    size_t capacity;
} string_t;

// Create an empty string_t, with default capacity
string_t string_create();

// Create an empty string_t, with given capacity
string_t string_create_with_capacity(size_t capacity);

// Free the string_t
void string_free(string_t* str);

// Create a string_t from a copy of a given string_t
string_t string_copy_from_string(const string_t* other);

// Create a string_t from a copy of a given null-terminated char*
string_t string_copy_from_char_arr(const char* other);

// Get the underlying char* null-terminated
char* string_get(string_t* str);

// Append the content of a string_t to another string_t
string_t* string_append_from_string(const string_t* src, string_t* dst);

// Append the content of a null-terminated char* to a string_t
string_t* string_append_from_char_arr(const char* src, string_t* dst);

// Format the given string_t
string_t* string_format(string_t* dst, const char* fmt, ...);

// Trim whitespaces at the start and end of the string_t
string_t* string_trim(string_t* str);

char* trim_whitespaces(char* src);
size_t tokenize_line(char* line, char* tokens[]);

#endif
