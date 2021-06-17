#ifndef STDLIB_H_
#define STDLIB_H_

#include <_internals.h>
#include <stdint.h>

__BEGIN_CDECLS

void abort();
void *malloc(size_t size);
void free(void *ptr);
void *realloc(void *ptr, size_t new_size);
void *calloc(size_t num, size_t size);
void *aligned_alloc(size_t alignment, size_t size);

int system(const char *cmd);

__END_CDECLS

#endif
