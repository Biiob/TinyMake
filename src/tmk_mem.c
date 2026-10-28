#include "tmk_mem.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

void* tmk_xrealloc(void* ptr, size_t size)
{
  assert(size > 0);

  void* tmp = realloc(ptr, size);
  if (tmp == NULL)
  {
    fprintf(stderr, "Could not allocate %zu bytes\n", size);
    abort();
  }

  return tmp;
}
