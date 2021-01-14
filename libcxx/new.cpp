#include <stddef.h>
#include <stdint.h>

#ifdef KERNEL
void *kmalloc(size_t size);
void *kmalloc(size_t size, uint32_t alignment);
void kfree(void *ptr);
#else
#error "Need user defined malloc implementations!"
#endif

namespace std {

enum class align_val_t : size_t {};

}  // namespace std

#ifdef KERNEL

void *operator new(size_t size) { return kmalloc(size); }

void *operator new(size_t size, std::align_val_t alignment) {
  return kmalloc(size, static_cast<uint32_t>(alignment));
}

void operator delete(void *ptr) { kfree(ptr); }

void operator delete(void *ptr, [[maybe_unused]] std::align_val_t alignment) {
  kfree(ptr);
}

#endif
