#include <kstring.h>

void *memset(void *ptr, int value, size_t size) {
  auto *p = static_cast<unsigned char *>(ptr);
  while (size--) {
    *p = static_cast<unsigned char>(value);
    ++p;
  }
  return ptr;
}

void *memcpy(void *dst, const void *src, size_t num) {
  const auto *p_src = static_cast<const unsigned char *>(src);
  auto *p_dst = static_cast<unsigned char *>(dst);
  while (num--) {
    *p_dst = *p_src;
    ++p_dst;
    ++p_src;
  }
  return dst;
}

size_t strlen(const char *str) {
  size_t len = 0;
  while (str[len]) len++;
  return len;
}

int strcmp(const char *s1, const char *s2) {
  while (*s1 && (*s1 == *s2)) {
    ++s1;
    ++s2;
  }
  return *s1 - *s2;
}
