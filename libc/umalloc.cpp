#include <allocator.h>

#include <cassert>
#include <cstdio>

namespace user {

namespace {

constexpr size_t kChunkSize = 1024;
const uint8_t *kHeapTop, *kHeapBottom;

utils::Allocator UserAllocator;

void *usbrk_chunk(size_t n, void *heap) {
  uint8_t *heap_bytes = reinterpret_cast<uint8_t *>(heap);
  assert(heap_bytes + (n * kChunkSize) <= kHeapTop &&
         "Attempting to allocate beyond the end of the heap.");
  return heap_bytes + n * kChunkSize;
}

// Just allocate in chunks, similar to what we do in the kernel.
void *usbrk(size_t n, void *heap) {
  assert(heap == UserAllocator.getHeap() &&
         "This should only be called once the heap top limit is reached.");
  assert(n && "Bad size request");

  if (n < kChunkSize) return usbrk_chunk(1, heap);

  if (n % kChunkSize == 0) return usbrk_chunk(n / kChunkSize, heap);

  return usbrk_chunk(n / kChunkSize + 1, heap);  // Round up.
}

}  // namespace

void InitializeUserHeap(uint8_t *heap_bottom, uint8_t *heap_top) {
  kHeapTop = heap_top;
  kHeapBottom = heap_bottom;
  UserAllocator.Init(heap_bottom, usbrk, heap_top);
}

void *umalloc(size_t size) { return UserAllocator.Malloc(size); }

void *umalloc(size_t size, uint32_t alignment) {
  return UserAllocator.Malloc(size, alignment);
}

void ufree(void *ptr) { return UserAllocator.Free(ptr); }

void *urealloc(void *ptr, size_t size) {
  return UserAllocator.Realloc(ptr, size);
}

void *ucalloc(size_t num, size_t size) {
  return UserAllocator.Calloc(num, size);
}

}  // namespace user

namespace utils {

size_t GetHeapUsed() { return user::UserAllocator.getHeapUsed(); }

}  // namespace utils

#ifdef __cplusplus
extern "C" {
#endif

void InitializeUserHeap(uint8_t *heap_bottom, uint8_t *heap_top) {
  return user::InitializeUserHeap(heap_bottom, heap_top);
}

#ifdef __cplusplus
}  // extern "C"
#endif
