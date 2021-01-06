#ifndef KHEAP_H_
#define KHEAP_H_

#include <kstdint.h>

/**
   Allocate a chunk of memory, size in size. The chunk will be
   page aligned.
**/
void *kmalloc_aligned(uint32_t size);

/**
   General allocation function.
**/
void *kmalloc(uint32_t size);

#endif
