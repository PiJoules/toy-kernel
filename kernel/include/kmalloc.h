#ifndef KMALLOC_H_
#define KMALLOC_H_

#include <stddef.h>
#include <stdint.h>
#include <type_traits.h>

void *kmalloc(size_t size);
void *kmalloc(size_t size, uint32_t alignment);
void kfree(void *ptr);
void *krealloc(void *ptr, size_t size);
void *kcalloc(size_t num, size_t size);

namespace toy {

// Allocate enough space for a given number of elements of a given type (T), and
// return a T*. The value is aligned to any fundamental type.
template <typename T>
T *kmalloc(size_t num_elems) {
  return reinterpret_cast<T *>(::kmalloc(num_elems * sizeof(T)));
}

template <typename T>
T *krealloc(T *ptr, size_t num_elems) {
  return reinterpret_cast<T *>(::krealloc(ptr, num_elems * sizeof(T)));
}

template <typename T>
T *kcalloc(size_t num_elems) {
  return reinterpret_cast<T *>(::kcalloc(num_elems, sizeof(T)));
}

}  // namespace toy

void InitializeKernelHeap();
size_t GetKernelHeapUsed();

#endif
