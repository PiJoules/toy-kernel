#include <string.h>

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
