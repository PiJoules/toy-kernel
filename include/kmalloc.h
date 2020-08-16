#ifndef KMALLOC_H_
#define KMALLOC_H_

#include <kstdint.h>
#include <ktype_traits.h>

constexpr size_t kMaxAlignment = 4;

void *kmalloc(size_t size);
void *kmalloc(size_t size, uint32_t alignment);
void kfree(void *v_addr);

namespace toy {

// Allocate enough space for a given number of elements of a given type (T), and
// return a T*. The value is aligned to any fundamental type.
template <typename T>
T *kmalloc(size_t num_elems) {
  return reinterpret_cast<T *>(::kmalloc(num_elems * sizeof(T)));
}

}  // namespace toy

void InitializeKernelHeap();
size_t GetKernelHeapUsed();

struct MallocHeader {
  unsigned size : 31;  // The size includes the size of this header.
  unsigned used : 1;

  static MallocHeader *FromPointer(void *ptr) {
    return reinterpret_cast<MallocHeader *>(reinterpret_cast<uintptr_t>(ptr) -
                                            sizeof(MallocHeader));
  }

  uint8_t *getOffset(size_t offset) {
    return reinterpret_cast<uint8_t *>(this) + offset;
  }

  uint8_t *getEnd() { return getOffset(size); }

  MallocHeader *NextChunk(size_t size) {
    return reinterpret_cast<MallocHeader *>(getOffset(size));
  }
  MallocHeader *NextChunk() {
    return reinterpret_cast<MallocHeader *>(getEnd());
  }
} __attribute__((packed));
static_assert(sizeof(MallocHeader) == 4, "");

constexpr size_t kMallocMinSize = sizeof(MallocHeader);

#endif
