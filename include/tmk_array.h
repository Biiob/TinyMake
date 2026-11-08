#ifndef TMK_ARRAY_H
#define TMK_ARRAY_H

#include <stddef.h>
#include <stdlib.h>

#include "tmk_mem.h"

#define TMK_ARRAY_MIN_CAP 8

// Declares a growable array type called name, holding elements of type type
// A zeroed struct is a valid empty array
// Use it once per array type, in the header that owns the element type
#define tmk_define_array(name, type) \
  typedef struct                     \
  {                                  \
    type* items;                     \
    size_t len;                      \
    size_t capacity;                 \
  } name

// The macros below take a pointer to the array and evaluate it several times,
// so that argument must not have side effects

// Stores item at the end of the array, which takes ownership of it
#define tmk_array_push(arr, item)                                    \
  do                                                                 \
  {                                                                  \
    if ((arr)->len == (arr)->capacity)                               \
    {                                                                \
      size_t tmk_cap_ = (arr)->capacity == 0 ? TMK_ARRAY_MIN_CAP     \
                                             : (arr)->capacity * 2;  \
      (arr)->items = tmk_xrealloc((arr)->items,                      \
                                  tmk_cap_ * sizeof(*(arr)->items)); \
      (arr)->capacity = tmk_cap_;                                    \
    }                                                                \
    (arr)->items[(arr)->len] = (item);                               \
    (arr)->len++;                                                    \
  } while (0)

// Frees the array but not what its elements own. Leaves arr zeroed
#define tmk_array_free(arr) \
  do                        \
  {                         \
    free((arr)->items);     \
    (arr)->items = NULL;    \
    (arr)->len = 0;         \
    (arr)->capacity = 0;    \
  } while (0)

// Calls free_item with a pointer to each element, then frees the array
// Passing free directly is wrong: it would get the address of the slot
#define tmk_array_free_with(arr, free_item)                \
  do                                                       \
  {                                                        \
    for (size_t tmk_i_ = 0; tmk_i_ < (arr)->len; ++tmk_i_) \
    {                                                      \
      free_item(&(arr)->items[tmk_i_]);                    \
    }                                                      \
    tmk_array_free(arr);                                   \
  } while (0)

#endif
