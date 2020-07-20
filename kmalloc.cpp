#include <Terminal.h>
#include <kassert.h>
#include <kmalloc.h>
#include <kstring.h>
#include <paging.h>
#include <panic.h>

namespace {

size_t KernelHeapUsed = 0;

// Virtual memory.
auto *KernelHeap = reinterpret_cast<char *>(KERN_HEAP_BEGIN);

void *ksbrk_page(unsigned n) {
  if ((KernelHeap + (n * kPageSize4M)) >
      reinterpret_cast<char *>(KERN_HEAP_END)) {
    // No virtual memory left for kernel heap!
    return nullptr;
  }

  auto *chunk = reinterpret_cast<MallocHeader *>(KernelHeap);
  for (unsigned i = 0; i < n; ++i) {
    uint8_t *p_addr = GetKernelPageDirectory().NextFreePhysicalPage();
    assert(p_addr && "No free page frames available!");

    // Add PG_USER to allow user programs to read kernel heap
    GetKernelPageDirectory().AddPage(KernelHeap, p_addr, PG_USER);
    // AddPageToPageDirectory(KernelHeap, p_addr, 0);

    KernelHeap += kPageSize4M;
  }

  chunk->size = kPageSize4M * n;
  chunk->used = 0;

  return chunk;
}

}  // namespace

void InitializeKernelHeap() { ksbrk_page(1); }

void *kmalloc(size_t size) { return kmalloc(size, kMaxAlignment); }

void *kmalloc(size_t size, uint32_t alignment) {
  assert(alignment && IsPowerOf2(alignment) && "Invalid alignment");
  if (size == 0) return nullptr;

  size_t realsize = sizeof(MallocHeader) + size;
  MallocHeader *chunk = reinterpret_cast<MallocHeader *>(KERN_HEAP_BEGIN);

  // TODO: Clean this up since it's hard to read.
  auto CanUseChunk = [realsize, alignment](const MallocHeader *chunk,
                                           size_t &adjust) {
    auto size = chunk->size;
    if (chunk->used || size < realsize) return false;

    if (size > realsize) {
      // We can potentially split this into chunks later, but only if both of
      // the chunks can fit a new MallocHeader. Otherwise, we could potentially
      // write over other data.
      if (size < sizeof(MallocHeader) * 2) return false;
    }

    // Check alignment.
    auto addr = reinterpret_cast<uintptr_t>(chunk) + sizeof(MallocHeader);
    adjust = (alignment - (addr % alignment)) % alignment;
    if (adjust == 0) {
      // Aligned.
      return size - realsize >= sizeof(MallocHeader);
    }

    // We can attempt to split the chunk into an unaligned segment followed by
    // an aligned one by addign the adjust, but we can only split it if the
    // unaligned segment can fit a malloc header.
    if (adjust < sizeof(MallocHeader)) return false;

    if (size < realsize + adjust) return false;

    // Unaligned, but there's room to split the chunk into 2 parts.
    return true;
  };

  // Keep searching until we find a chunk that's available and can fit the
  // allocation we want.
  size_t adjust = 0;
  while (!CanUseChunk(chunk, adjust)) {
    assert(chunk->size &&
           "Corrupted chunk marked as used but has 0 heap size.");

    chunk = chunk->NextChunk();

    assert(chunk <= reinterpret_cast<MallocHeader *>(KernelHeap) &&
           "Found a chunk that was allocated past the kernel heap limit.");
    if (chunk == reinterpret_cast<MallocHeader *>(KernelHeap)) {
      // Attempt to allocate more if we reached the end of the allocated heap.
      assert(ksbrk_page((realsize / kPageSize4M) + 1) &&
             "No memory left for kernel!");
    }
  }

  if (adjust) {
    // This chunk has enough space, but we need to return an address somewhere
    // inside the chunk that's aligned properly. We will need to split this one
    // into 1 unaligned chunk followed by 1 aligned chunk.
    auto *other = chunk->NextChunk(adjust);
    other->size = chunk->size - adjust;

    assert(adjust >= sizeof(MallocHeader) &&
           "Cannot create a chunk less than sizeof(MallocHeader)");
    chunk->size = adjust;
    chunk->used = 0;

    // We will return the other chunk.
    chunk = other;
  }

  // Found an unused chunk at this point that can fit our allocation. This chunk
  // could be new or have been previously allocated but freed.
  if (chunk->size == realsize) {
    // Fits perfectly.
    chunk->used = 1;
  } else {
    // Need to create another chunk.
    auto *other = chunk->NextChunk(realsize);

    other->size = chunk->size - realsize;
    assert(other->size >= sizeof(MallocHeader) &&
           "Created chunk with invalid size");
    other->used = 0;

    chunk->size = realsize;
    chunk->used = 1;
  }

  KernelHeapUsed += realsize;
  void *ptr = reinterpret_cast<uint8_t *>(chunk) + sizeof(MallocHeader);
  assert(reinterpret_cast<uintptr_t>(ptr) % alignment == 0 &&
         "Returning an unaligned pointer!");
  return ptr;
}

void kfree(void *v_addr) {
  if (!v_addr) return;

  auto *chunk = MallocHeader::FromPointer(v_addr);
  chunk->used = 0;

  assert(KernelHeapUsed >= chunk->size &&
         "Attempting to free more memory than was recorded");
  KernelHeapUsed -= chunk->size;

  // Merge free block with next free block
  MallocHeader *other;
  while ((other = chunk->NextChunk()) &&
         other < reinterpret_cast<MallocHeader *>(KernelHeap) &&
         other->used == 0) {
    chunk->size += other->size;
  }
}

size_t GetKernelHeapUsed() { return KernelHeapUsed; }
