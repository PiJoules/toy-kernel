#include <stddef.h>
#include <stdint.h>

#include <cstdlib>
#include <new>

void *operator new(size_t size) { return std::malloc(size); }

void *operator new(size_t size, std::align_val_t alignment) {
  return std::aligned_alloc(static_cast<uint32_t>(alignment), size);
}

void *operator new[](size_t size) { return std::malloc(size); }

void operator delete(void *ptr) { std::free(ptr); }
void operator delete[](void *ptr) { std::free(ptr); }

void operator delete(void *ptr, [[maybe_unused]] std::align_val_t alignment) {
  std::free(ptr);
}
