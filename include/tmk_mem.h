#ifndef TMK_MEM_H
#define TMK_MEM_H

#include <stddef.h>

// realloc that aborts instead of returning NULL.
// ptr may be NULL
// size must not be 0
void* tmk_xrealloc(void* ptr, size_t size);

#endif
