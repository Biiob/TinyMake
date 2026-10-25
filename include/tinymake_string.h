#ifndef TINYMAKE_STRING
#define TINYMAKE_STRING

#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>

#define DEFAULT_STRING_SIZE 256
typedef struct
{
  char* str;
  size_t len;
  size_t capacity;
} tmk_str;

// Create an empty tmk_str, with default capacity
tmk_str tmk_str_create(void);

// Create an empty tmk_str, with given capacity
tmk_str tmk_str_create_with_capacity(size_t capacity);

// Allocate given capacity for the string
void tmk_str_reserve(tmk_str* str, size_t capacity);

// Clears the content of the string
void tmk_str_clear(tmk_str* str);

// Free the tmk_str
void tmk_str_free(tmk_str* str);

// Create a tmk_str from a copy of a given tmk_str
tmk_str tmk_str_clone(const tmk_str* other);

// Create a tmk_str from a copy of a given null-terminated char*
tmk_str tmk_str_from_cstr(const char* other);

// Create a tmk_str and take ownership of the given null-terminated char*
tmk_str tmk_str_take(char* str);

// Release ownership of the underlying char* and resets the tmk_str
char* tmk_str_release(tmk_str* str);

// Returns the character at index i, with bounds checking
const char* tmk_str_at(const tmk_str* str, size_t i);

// Set the character at index i, with bounds checking
void tmk_str_set(tmk_str* str, size_t i, char c);

// Get the underlying char* null-terminated
const char* tmk_str_cstr(const tmk_str* str);

// Append the content of a tmk_str to another tmk_str
tmk_str* tmk_str_append(tmk_str* dst, const tmk_str* src);

// Append the content of a null-terminated char* to a tmk_str
tmk_str* tmk_str_append_cstr(tmk_str* dst, const char* src);

// Appends n characters from a buffer to a tmk_str
tmk_str* tmk_str_append_n(tmk_str* dst, const char* src, size_t n);

// Appends a single character to a tmk_str
tmk_str* tmk_str_push(tmk_str* dst, char c);

// Format then append the provided string with args in va_list
tmk_str* tmk_str_vappend_format(tmk_str* dst, const char* fmt, va_list args);

// Format then append the provided string
tmk_str* tmk_str_append_format(tmk_str* dst, const char* fmt, ...);

// Format the given tmk_str with args in va_list
tmk_str* tmk_str_vformat(tmk_str* dst, const char* fmt, va_list args);

// Format the given tmk_str
tmk_str* tmk_str_format(tmk_str* dst, const char* fmt, ...);

// Trim whitespaces at the start and end of the tmk_str
tmk_str* tmk_str_trim(tmk_str* str);

// Compares if two tmk_str are equal
bool tmk_str_equal(const tmk_str* str1, const tmk_str* str2);

// Compares if two tmk_str are equal, ignoring case
bool tmk_str_equal_nocase(const tmk_str* str1, const tmk_str* str2);

char* trim_whitespaces(char* src);
size_t tokenize_line(char* line, char* tokens[]);

#endif
