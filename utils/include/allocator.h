#ifndef ALLOCATOR_H_
#define ALLOCATOR_H_

#include <cassert>
#include <cstddef>
#include <cstdint>

// The allocator is in charge of providing a malloc/free-like interface.

namespace utils {

struct MallocHeader {
  // The size includes the size of this header. This size may also not be the
  // exact same size as the size requested (ie. it could be a little more than
  // what was requested).
  //
  // USERS SHOULD NOT EXPECT `size` TO HOLD THE REQUESTED MALLOC SIZE.
  unsigned size : 31;
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
static_assert(sizeof(MallocHeader) == kMaxAlignment, "");

constexpr size_t kMallocMinSize = sizeof(MallocHeader);

class Allocator {
 public:
  // Request the heap top to be moved up by `increment` bytes. `heap` is the
  // current value of the heap. This function returns the new value of the heap
  // top after this function is called.
  //
  // If we cannot increment from running out of space, nullptr is returned.
  //
  // On a successful return (non-nullptr), the new heap top returned can
  // increase more than what was the request amount.
  using SbrkFunc = void *(*)(size_t increment, void *heap);

  Allocator(void *heap_start, SbrkFunc func, void *heap_end = nullptr)
      : heap_(heap_start),
        sbrk_(func),
        heap_start_(heap_start),
        heap_end_(heap_end) {
    InitializeHeap();
  }

  // This is used by the kernel for creating a global allocator to be
  // initialized later via `Init`.
  Allocator() {}
  void Init(void *heap_start, SbrkFunc func, void *heap_end = nullptr) {
    heap_ = heap_start;
    sbrk_ = func;
    heap_end_ = heap_end;
    heap_used_ = 0;
    heap_start_ = heap_start;

    InitializeHeap();
  }

  void *Malloc(size_t size);
  void *Malloc(size_t size, uint32_t alignment);
  void *Realloc(void *ptr, size_t size);
  void Free(void *ptr);
  void *Calloc(size_t num, size_t size);

  size_t getHeapUsed() const { return heap_used_; }
  void *getHeap() const { return heap_; }

 private:
  void InitializeHeap() {
    if (heap_end_) assert(heap_end_ > heap_start_);

    // Request just 1 byte for now. This also sets up the first chunk.
    heap_ = sbrk_(1, heap_);
    assert(heap_ > heap_start_);

    auto *first_chunk = reinterpret_cast<MallocHeader *>(heap_start_);
    first_chunk->used = 0;
    first_chunk->size = reinterpret_cast<uint8_t *>(heap_) -
                        reinterpret_cast<uint8_t *>(heap_start_);
  }

  /**
   * Perform a realloc by performing a malloc for the new size, copying over the
   * data, and freeing the original.
   */
  void *SlowRealloc(void *ptr, size_t size);

  void *heap_;  // Top of the heap.
  SbrkFunc sbrk_;
  size_t heap_used_ = 0;
  void *heap_start_;

  // This is mainly used for sanity checks. If this value is non-null, then
  // we should check that the internal heap never exceeds this value. Otherwise,
  // there is no (practical) end to the heap and we can just keep allocating.
  void *heap_end_ = nullptr;
};

size_t GetHeapUsed();

}  // namespace utils

#endif
