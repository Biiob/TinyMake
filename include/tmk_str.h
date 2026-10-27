#ifndef TMK_STR_H
#define TMK_STR_H

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>

#define TMK_STR_DEFAULT_CAP 32

// Passed as a length to mean "until the end of the string"
#define TMK_STR_NPOS SIZE_MAX

// Owning, growable string. str is always null-terminated and capacity counts
// the terminator. A plain struct copy shares the buffer: only one of the copies
// may be freed.
// Functions returning tmk_str* return dst/str, to allow chaining.
typedef struct
{
  char* str;
  size_t len;
  size_t capacity;
} tmk_str;

tmk_str tmk_str_create(void);

tmk_str tmk_str_create_with_capacity(size_t capacity);

// Grows to exactly capacity, never shrinks
void tmk_str_reserve(tmk_str* str, size_t capacity);

// Empties the string, keeping its capacity
void tmk_str_clear(tmk_str* str);

// Leaves str zeroed, so freeing twice is safe
void tmk_str_free(tmk_str* str);

tmk_str tmk_str_clone(const tmk_str* other);

// Copies n bytes of buf, which does not need to be null-terminated
tmk_str tmk_str_from_buf(const char* buf, size_t n);

tmk_str tmk_str_from_cstr(const char* other);

// Copies at most n characters from pos, or until the end with TMK_STR_NPOS
// Returns an empty string if pos is past the end
tmk_str tmk_str_substr(const tmk_str* str, size_t pos, size_t n);

// Takes ownership of a malloc'd null-terminated string, without copying
tmk_str tmk_str_take(char* str);

// Gives up ownership of the buffer, which the caller must free()
// str is left zeroed, like after tmk_str_free
char* tmk_str_release(tmk_str* str);

// Does nothing if i >= len or c is '\0'
void tmk_str_set(tmk_str* str, size_t i, char c);

const char* tmk_str_cstr(const tmk_str* str);

// Writable buffer of capacity bytes. Call tmk_str_set_len after writing to it
char* tmk_str_data(tmk_str* str);

// Sets len and the terminator after writing through tmk_str_data
// len must be strictly less than capacity
void tmk_str_set_len(tmk_str* str, size_t len);

tmk_str* tmk_str_append(tmk_str* dst, const tmk_str* src);

tmk_str* tmk_str_append_cstr(tmk_str* dst, const char* src);

tmk_str* tmk_str_append_n(tmk_str* dst, const char* src, size_t n);

// Does nothing if c is '\0'
tmk_str* tmk_str_push(tmk_str* dst, char c);

// Format functions return NULL on an encoding error
// The arguments must not point inside dst
tmk_str* tmk_str_vappend_format(tmk_str* dst, const char* fmt, va_list args);
tmk_str* tmk_str_append_format(tmk_str* dst, const char* fmt, ...);

// Same as the append versions, but replaces the content of dst
tmk_str* tmk_str_vformat(tmk_str* dst, const char* fmt, va_list args);
tmk_str* tmk_str_format(tmk_str* dst, const char* fmt, ...);

// Removes leading and trailing whitespace, in place
tmk_str* tmk_str_trim(tmk_str* str);

bool tmk_str_equal(const tmk_str* str1, const tmk_str* str2);
bool tmk_str_equal_cstr(const tmk_str* str1, const char* str2);
bool tmk_str_equal_nocase(const tmk_str* str1, const tmk_str* str2);

// Splits str on delim, skipping empty tokens, and returns the token count
// *toks is a malloc'd array (NULL if no token): the caller must free each
// token, then the array
size_t tmk_str_tokenize(tmk_str** toks, const tmk_str* str, char delim);

// Find functions return the index of c, or len if not found
size_t tmk_str_find_char(const tmk_str* str, char c);
size_t tmk_str_rfind_char(const tmk_str* str, char c);

// Replaces the content of dst with the next line of f, '\n' included
// Returns the line length, or -1 on EOF or error, leaving dst empty
ssize_t tmk_str_read_line(tmk_str* dst, FILE* f);

#endif
