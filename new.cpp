#include <kmalloc.h>

namespace std {

enum class align_val_t : size_t {};

}  // namespace std

void *operator new(size_t size) { return kmalloc(size); }

void *operator new(size_t size, std::align_val_t alignment) {
  return kmalloc(size, static_cast<uint32_t>(alignment));
}

void operator delete(void *ptr) { kfree(ptr); }

void operator delete(void *ptr, std::align_val_t alignment) { kfree(ptr); }
