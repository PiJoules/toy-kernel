#ifndef STDLIB_H_
#define STDLIB_H_

#include <stdint.h>

[[noreturn]] void abort();
void *malloc(size_t size);
void free(void *ptr);
void *realloc(void *ptr, size_t new_size);
void *calloc(size_t num, size_t size);
void *malloc(size_t size, uint32_t alignment);

#endif
