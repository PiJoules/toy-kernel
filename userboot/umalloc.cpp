#include <allocator.h>
#include <assert.h>

extern "C" uint8_t heap_bottom, heap_top;

namespace user {

namespace {

constexpr size_t kChunkSize = 1024;

void *usbrk_chunk(size_t n, void *&heap) {
  uint8_t *&heap_bytes = reinterpret_cast<uint8_t *&>(heap);
  assert(heap_bytes + (n * kChunkSize) <= &heap_top &&
         "Attempting to allocate beyond the end of the heap.");

  auto *chunk = reinterpret_cast<utils::MallocHeader *>(heap_bytes);
  heap_bytes += n * kChunkSize;

  chunk->size = kChunkSize * n;
  chunk->used = 0;

  return chunk;
}

// Just allocate in chunks, similar to what we do in the kernel.
void *usbrk(size_t n, void *&heap) {
  if (n < kChunkSize) return usbrk_chunk(1, heap);

  if (n % kChunkSize == 0) return usbrk_chunk(n / kChunkSize, heap);

  return usbrk_chunk(n / kChunkSize + 1, heap);  // Round up.
}

utils::Allocator UserAllocator;

}  // namespace

void InitializeUserHeap() {
  UserAllocator.Init(&heap_bottom, usbrk, &heap_top);
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
