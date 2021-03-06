#include <stdlib.h>
#include <string.h>

void *memset(void *ptr, int value, size_t size) {
  auto *p = static_cast<unsigned char *>(ptr);
  while (size--) {
    *p = static_cast<unsigned char>(value);
    ++p;
  }
  return ptr;
}

// TODO: We should do something similar to what glibc does and take advantage of
// larger reads/stores by starting them at properly aligned addresses. All other
// addresses should continue with a byte-by-byte read.
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

void *memmove(void *dest, const void *src, size_t size) {
  if (!size) return dest;
  char *temp = (char *)malloc(size);
  memcpy(temp, src, size);
  memcpy(dest, temp, size);
  free(temp);
  return dest;
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

int memcmp(const void *lhs, const void *rhs, size_t size) {
  const auto *v1 = reinterpret_cast<const unsigned char *>(lhs);
  const auto *v2 = reinterpret_cast<const unsigned char *>(rhs);
  while (size && (*v1 == *v2)) {
    ++v1;
    ++v2;
    --size;
  }
  return *v1 - *v2;
}
