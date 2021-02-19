#ifndef STRING_H_
#define STRING_H_

#include <stdint.h>

extern "C" {

void *memcpy(void *dst, const void *src, size_t num);
void *memset(void *ptr, int value, size_t size);
void *memmove(void *dest, const void *src, size_t size);
size_t strlen(const char *str);
int strcmp(const char *s1, const char *s2);

}  // extern "C"

#endif
