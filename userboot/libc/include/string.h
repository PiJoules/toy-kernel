#ifndef STRING_H_
#define STRING_H_

#include <stdint.h>

extern "C" {

void *memcpy(void *dst, const void *src, size_t num);
void *memset(void *ptr, int value, size_t size);

}  // extern "C"

#endif
