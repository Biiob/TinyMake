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
} string_t;

// Create an empty string_t, with default capacity
string_t string_create(void);

// Create an empty string_t, with given capacity
string_t string_create_with_capacity(size_t capacity);

// Allocate given capacity for the string
void string_reserve(string_t* str, size_t capacity);

// Clears the content of the string
void string_clear(string_t* str);

// Free the string_t
void string_free(string_t* str);

// Create a string_t from a copy of a given string_t
string_t string_clone(const string_t* other);

// Create a string_t from a copy of a given null-terminated char*
string_t string_from_cstr(const char* other);

// Create a string_t and take ownership of the given null-terminated char*
string_t string_take(char* str);

// Release ownership of the underlying char* and resets the string_t
char* string_release(string_t* str);

// Returns the character at index i, with bounds checking
const char* string_at(const string_t* str, size_t i);

// Set the character at index i, with bounds checking
void string_set(string_t* str, size_t i, char c);

// Get the underlying char* null-terminated
const char* string_cstr(const string_t* str);

// Append the content of a string_t to another string_t
string_t* string_append(string_t* dst, const string_t* src);

// Append the content of a null-terminated char* to a string_t
string_t* string_append_cstr(string_t* dst, const char* src);

// Appends n characters from a buffer to a string_t
string_t* string_append_n(string_t* dst, const char* src, size_t n);

// Appends a single character to a string_t
string_t* string_push(string_t* dst, char c);

// Format then append the provided string with args in va_list
string_t* string_vappend_format(string_t* dst, const char* fmt, va_list args);

// Format then append the provided string
string_t* string_append_format(string_t* dst, const char* fmt, ...);

// Format the given string_t with args in va_list
string_t* string_vformat(string_t* dst, const char* fmt, va_list args);

// Format the given string_t
string_t* string_format(string_t* dst, const char* fmt, ...);

// Trim whitespaces at the start and end of the string_t
string_t* string_trim(string_t* str);

// Compares if two string_t are equal
bool string_equal(const string_t* str1, const string_t* str2);

// Compares if two string_t are equal, ignoring case
bool string_equal_nocase(const string_t* str1, const string_t* str2);

char* trim_whitespaces(char* src);
size_t tokenize_line(char* line, char* tokens[]);

#endif
