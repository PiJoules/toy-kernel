#include <stddef.h>
#include <stdint.h>

#include <cstdlib>

namespace std {

enum class align_val_t : size_t {};

}  // namespace std

void *operator new(size_t size) { return std::malloc(size); }

void *operator new(size_t size, std::align_val_t alignment) {
  return std::malloc(size, static_cast<uint32_t>(alignment));
}

void *operator new[](size_t size) { return std::malloc(size); }

void operator delete(void *ptr) { std::free(ptr); }
void operator delete[](void *ptr) { std::free(ptr); }

void operator delete(void *ptr, [[maybe_unused]] std::align_val_t alignment) {
  std::free(ptr);
}
