#ifndef KSTRING_H_
#define KSTRING_H_

#include <kstdint.h>

extern "C" {
void *memset(void *ptr, int value, size_t size);
void *memcpy(void *dst, const void *src, size_t num);
size_t strlen(const char *str);
int strcmp(const char *s1, const char *s2);
}

#endif
