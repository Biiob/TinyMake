#ifndef TMK_STR_ARRAY_H
#define TMK_STR_ARRAY_H

#include "tmk_str.h"

// Growable array owning its strings. A zeroed struct is a valid empty array
typedef struct
{
  tmk_str* items;
  size_t len;
  size_t capacity;
} tmk_str_array;

// Takes ownership of str: the caller must not free it afterwards
void tmk_str_array_push(tmk_str_array* arr, tmk_str str);

// Moves the string at i out of the array, leaving a zeroed string in its slot
// len is unchanged, and the slot can still be freed safely
tmk_str tmk_str_array_take(tmk_str_array* arr, size_t i);

// Frees every string, then the array. Leaves arr zeroed
void tmk_str_array_free(tmk_str_array* arr);

// Splits str on delim, skipping empty tokens
tmk_str_array tmk_str_tokenize(const tmk_str* str, char delim);

#endif
