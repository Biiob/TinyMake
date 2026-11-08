#include "tmk_array.h"

#include <assert.h>
#include <stdbool.h>
#include <string.h>

#include "tmk_array_test.h"

#define N_TRACKED 40

typedef struct
{
  int id;
  // Number of times each element was freed, indexed by id
  int* n_frees;
} tracked;

tmk_define_array(int_array, int);
tmk_define_array(cstr_array, const char*);
tmk_define_array(tracked_array, tracked);

static void free_tracked(tracked* t)
{
  t->n_frees[t->id]++;
}

static int next_value(int* counter)
{
  return (*counter)++;
}

void test_array_zeroed(void)
{
  // A zeroed array is empty and can be freed, even twice
  int_array arr = {0};
  assert(arr.len == 0);
  tmk_array_free(&arr);
  tmk_array_free(&arr);
  assert(arr.items == NULL);
}

void test_array_push(void)
{
  int_array arr = {0};

  tmk_array_push(&arr, 42);
  assert(arr.len == 1);
  assert(arr.capacity == TMK_ARRAY_MIN_CAP);
  assert(arr.items[0] == 42);

  tmk_array_free(&arr);
}

void test_array_push_grows(void)
{
  int_array arr = {0};

  for (int i = 0; i < TMK_ARRAY_MIN_CAP; ++i)
  {
    tmk_array_push(&arr, i);
  }
  assert(arr.capacity == TMK_ARRAY_MIN_CAP);

  tmk_array_push(&arr, TMK_ARRAY_MIN_CAP);
  assert(arr.capacity == TMK_ARRAY_MIN_CAP * 2);

  for (int i = TMK_ARRAY_MIN_CAP + 1; i < 100; ++i)
  {
    tmk_array_push(&arr, i);
  }

  assert(arr.len == 100);
  assert(arr.capacity >= arr.len);
  for (int i = 0; i < 100; ++i)
  {
    assert(arr.items[i] == i);
  }

  tmk_array_free(&arr);
}

void test_array_push_pointer_elements(void)
{
  // The element type does not have to be a single identifier
  cstr_array arr = {0};

  tmk_array_push(&arr, "first");
  tmk_array_push(&arr, "second");
  assert(arr.len == 2);
  assert(strcmp(arr.items[0], "first") == 0);
  assert(strcmp(arr.items[1], "second") == 0);

  tmk_array_free(&arr);
}

void test_array_push_evaluates_item_once(void)
{
  int_array arr = {0};
  int counter = 0;

  // Enough pushes to go through the growing path as well
  for (int i = 0; i < 100; ++i)
  {
    tmk_array_push(&arr, next_value(&counter));
    assert(counter == i + 1);
    assert(arr.items[i] == i);
  }

  tmk_array_free(&arr);
}

void test_array_macros_are_statements(void)
{
  int_array arr = {0};
  bool push = false;

  // No braces on purpose: each macro must expand to a single statement
  if (push)
    tmk_array_push(&arr, 1);
  else
    assert(arr.len == 0);
  assert(arr.len == 0);

  push = true;
  if (push)
    tmk_array_push(&arr, 2);
  else
    assert(false);
  assert(arr.len == 1);

  if (!push)
    tmk_array_free(&arr);
  else
    assert(arr.len == 1);
  assert(arr.items != NULL);

  tmk_array_free(&arr);
}

void test_array_free(void)
{
  int_array arr = {0};
  tmk_array_push(&arr, 1);

  tmk_array_free(&arr);
  assert(arr.items == NULL);
  assert(arr.len == 0);
  assert(arr.capacity == 0);

  // Still a valid array
  tmk_array_push(&arr, 2);
  assert(arr.len == 1);
  assert(arr.items[0] == 2);

  tmk_array_free(&arr);
}

void test_array_free_with(void)
{
  tracked_array arr = {0};
  int n_frees[N_TRACKED] = {0};

  for (int i = 0; i < N_TRACKED; ++i)
  {
    tmk_array_push(&arr, ((tracked){.id = i, .n_frees = n_frees}));
  }

  tmk_array_free_with(&arr, free_tracked);

  for (int i = 0; i < N_TRACKED; ++i)
  {
    assert(n_frees[i] == 1);
  }
  assert(arr.items == NULL);
  assert(arr.len == 0);
  assert(arr.capacity == 0);

  // Nothing left to free the second time
  tmk_array_free_with(&arr, free_tracked);
  for (int i = 0; i < N_TRACKED; ++i)
  {
    assert(n_frees[i] == 1);
  }
}

void test_array_free_with_empty(void)
{
  tracked_array arr = {0};
  tmk_array_free_with(&arr, free_tracked);
  assert(arr.items == NULL);
}

void test_array_free_with_indexed_argument(void)
{
  // The argument must not be confused with a variable inside the macro
  tracked_array groups[2] = {{0}, {0}};
  int n_frees[2] = {0};
  tmk_array_push(&groups[0], ((tracked){.id = 0, .n_frees = n_frees}));
  tmk_array_push(&groups[1], ((tracked){.id = 1, .n_frees = n_frees}));

  size_t i = 1;
  tmk_array_free_with(&groups[i], free_tracked);

  assert(i == 1);
  assert(n_frees[0] == 0);
  assert(n_frees[1] == 1);
  assert(groups[0].len == 1);
  assert(groups[1].len == 0);

  tmk_array_free_with(&groups[0], free_tracked);
}
