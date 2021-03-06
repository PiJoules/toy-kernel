#include <MathUtils.h>
#include <allocator.h>

#include <cassert>
#include <cstddef>
#include <cstring>

namespace utils {

void *Allocator::Malloc(size_t size) { return Malloc(size, kMaxAlignment); }

void *Allocator::Malloc(size_t size, uint32_t alignment) {
  assert(alignment && utils::IsPowerOf2(alignment) && "Invalid alignment");

  if (size == 0) return nullptr;

  size_t realsize = sizeof(MallocHeader) + size;
  MallocHeader *chunk = reinterpret_cast<MallocHeader *>(heap_start_);

  // See if a given chunk can hold a requested size with a given alignment. In
  // some cases, a chunk can be unaligned at first, but still fit the size
  // requested even if it is aligned by a certain amount. In this case, we can
  // store the adjust amount for later.
  //
  // `adjust` represents the number of bytes we can add to the chunk such that
  // the chunk can be aligned to `alignment`.
  //
  // TODO: Clean this up since it's hard to read.
  auto CanUseChunk = [realsize, alignment](const MallocHeader *chunk,
                                           size_t &adjust,
                                           MallocHeader *last_chunk) {
    auto size = chunk->size;
    if (chunk->used || size < realsize) return false;

    // Check alignment.
    auto addr = reinterpret_cast<uintptr_t>(chunk) + sizeof(MallocHeader);
    adjust = (alignment - (addr % alignment)) % alignment;
    if (adjust == 0) {
      // Aligned.
      return size - realsize >= sizeof(MallocHeader);
    }

    // We can potentially merge this chunk with the previous one when performing
    // adjustments, but only if the last chunk is not used.
    if (last_chunk && last_chunk->used) return false;

    if (size < realsize + adjust) return false;

    // Unaligned, but there's room to split the chunk into 2 parts.
    return true;
  };

  // Keep searching until we find a chunk that's available and can fit the
  // allocation we want.
  size_t adjust = 0;
  MallocHeader *last_chunk = nullptr;
  while (!CanUseChunk(chunk, adjust, last_chunk)) {
    assert(chunk->size &&
           "Corrupted chunk marked as used but has 0 heap size.");

    last_chunk = chunk;
    chunk = chunk->NextChunk();

    assert(chunk <= reinterpret_cast<MallocHeader *>(heap_) &&
           "Found a chunk that was allocated past the kernel heap limit.");
    if (chunk == reinterpret_cast<MallocHeader *>(heap_)) {
      // Attempt to allocate more if we reached the end of the allocated heap.
      assert(sbrk_(realsize, heap_) && "No memory left for kernel!");
    }
  }

  if (adjust && adjust < sizeof(MallocHeader)) {
    // Because this chunk is not small enough to fit a malloc header, we cannot
    // split it into an "adjust chunk" and the normal chunk we want. What we can
    // do instead though is merge this "adjust chunk" with the previous one and
    // return the new chunk normally. If the previous chunk was used, then this
    // would imply more of the heap is being used.
    assert(chunk != heap_start_ &&
           "No previous chunk before the start of the heap.");
    assert(last_chunk);
    assert(last_chunk->NextChunk() == chunk);

    size_t size = chunk->size;
    assert(size >= sizeof(MallocHeader));
    assert(size - adjust >= sizeof(MallocHeader));

    last_chunk->size += adjust;
    assert(!last_chunk->used &&
           "Cannot merge the adjust chunk with the previous one since this "
           "will change the heap usage size.");

    chunk = last_chunk->NextChunk();
    chunk->size = size - adjust;
    chunk->used = 0;
  } else if (adjust) {
    // This chunk has enough space, but we need to return an address somewhere
    // inside the chunk that's aligned properly. We will need to split this one
    // into one unaligned chunk followed by one aligned chunk.
    auto *other = chunk->NextChunk(adjust);
    other->size = chunk->size - adjust;

    assert(adjust >= sizeof(MallocHeader) &&
           "Cannot create a chunk less than sizeof(MallocHeader)");
    chunk->size = adjust;
    chunk->used = 0;

    // We will return the other chunk.
    chunk = other;
  }

  assert(chunk->size >= realsize);

  // Found an unused chunk at this point that can fit our allocation. This chunk
  // could be new or have been previously allocated but freed.
  if (chunk->size == realsize) {
    // Fits perfectly.
    chunk->used = 1;
  } else {
    // Need to create another chunk.
    auto *other = chunk->NextChunk(realsize);

    if (chunk->size - realsize < sizeof(MallocHeader)) {
      // We cannot split this into two chunks, but that's ok. We can just keep
      // this as one chunk.
      chunk->used = 1;
    } else {
      other->size = chunk->size - realsize;
      other->used = 0;

      chunk->size = realsize;
      chunk->used = 1;
    }
  }

  heap_used_ += chunk->size;

  void *ptr = reinterpret_cast<uint8_t *>(chunk) + sizeof(MallocHeader);
  assert(reinterpret_cast<uintptr_t>(ptr) % alignment == 0 &&
         "Returning an unaligned pointer!");

  return ptr;
}

void *Allocator::SlowRealloc(void *ptr, size_t size) {
  auto *chunk = MallocHeader::FromPointer(ptr);
  size_t oldsize = chunk->size - sizeof(MallocHeader);
  void *newptr = Malloc(size);
  assert(newptr != ptr &&
         "Somehow returned the same pointer. We should've already checked for "
         "this.");

  size_t cpysize = (size < oldsize) ? size : oldsize;
  memcpy(newptr, ptr, cpysize);
  Free(ptr);
  return newptr;
}

// Note that returning `nullptr` indicates that storage was not allocated and
// the pointer initially passed is still dereferencable.
void *Allocator::Realloc(void *ptr, size_t size) {
  assert(heap_start_ <= ptr && "This address was not allocated on the heap");
  if (heap_end_) {
    assert(ptr < heap_end_ && "This address was not allocated on the heap");
  }

  if (size == 0) return nullptr;

  auto *chunk = MallocHeader::FromPointer(ptr);
  assert(chunk->used && "Cannot realloc an unmalloc'd pointer");

  size_t realsize = size + sizeof(MallocHeader);

  if (chunk->size == realsize)
    // Size does not need to change.
    return ptr;

  if (chunk->size > realsize) {
    // Requesting a size decrease. Check to see if we can split this chunk into
    // two chunks. We can split into two if both chunks can hold at least a
    // MallocHeader. Otherwise, we could quickly merge the leftovers into the
    // next chunk if it is unused. This is primarily a shortcut.
    if (chunk->size > realsize + sizeof(MallocHeader)) {
      // Can split into two chunks.
      auto *other = chunk->NextChunk(realsize);
      other->size = chunk->size - realsize;
      other->used = 0;

      chunk->size = realsize;
      heap_used_ -= other->size;
      return ptr;
    }
  }

  return SlowRealloc(ptr, size);
}

void Allocator::Free(void *v_addr) {
  if (!v_addr) return;

  auto *chunk = MallocHeader::FromPointer(v_addr);
  chunk->used = 0;

  assert(heap_used_ >= chunk->size &&
         "Attempting to free more memory than was recorded");
  heap_used_ -= chunk->size;

  // Merge free block with next free block.
  MallocHeader *other;
  while ((other = chunk->NextChunk()) &&
         other < reinterpret_cast<MallocHeader *>(heap_) && other->used == 0) {
    chunk->size += other->size;
  }
}

void *Allocator::Calloc(size_t num, size_t size) {
  // TODO: Something better than this.
  void *ptr = Malloc(num * size);
  memset(ptr, 0, num * size);
  return ptr;
}

}  // namespace utils
