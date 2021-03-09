#include <allocator.h>
#include <assert.h>
#include <kmalloc.h>
#include <paging.h>
#include <panic.h>

namespace {

void *ksbrk_page(size_t n, void *heap) {
  uint8_t *heap_bytes = reinterpret_cast<uint8_t *>(heap);
  if ((heap_bytes + (n * kPageSize4M)) >
      reinterpret_cast<uint8_t *>(KERN_HEAP_END)) {
    // No virtual memory left for kernel heap!
    return nullptr;
  }

  for (unsigned i = 0; i < n; ++i) {
    // FIXME: We skip the first 4MB because we could still need to read stuff
    // that multiboot inserted in the first 4MB page. Starting from 0 here could
    // lead to overwriting that multiboot data. We should probably copy that
    // data somewhere else after paging is enabled.
    uint8_t *p_addr = GetPhysicalBitmap4M().NextFreePhysicalPage(/*start=*/1);
    assert(p_addr && "No free page frames available!");

    GetKernelPageDirectory().AddPage(heap_bytes, p_addr, /*flags=*/0);

    heap_bytes += kPageSize4M;
  }

  return heap_bytes;
}

void *ksbrk(size_t bytes, void *heap) {
  if (bytes < kPageSize4M) {
    // Always increase by at least 1 page.
    return ksbrk_page(1, heap);
  }

  if (bytes % kPageSize4M == 0) return ksbrk_page(bytes / kPageSize4M, heap);

  // Round up to nearest page if not even.
  return ksbrk_page(bytes / kPageSize4M + 1, heap);
}

utils::Allocator KernelAllocator;

}  // namespace

void InitializeKernelHeap() {
  KernelAllocator.Init((void *)KERN_HEAP_BEGIN, ksbrk, (void *)KERN_HEAP_END);
}

void *kmalloc(size_t size) {
  DisableInterruptsRAII disable_interrupts_raii;
  return KernelAllocator.Malloc(size);
}

void *kmalloc(size_t size, uint32_t alignment) {
  DisableInterruptsRAII disable_interrupts_raii;
  return KernelAllocator.Malloc(size, alignment);
}

void kfree(void *ptr) {
  DisableInterruptsRAII disable_interrupts_raii;
  return KernelAllocator.Free(ptr);
}

void *krealloc(void *ptr, size_t size) {
  DisableInterruptsRAII disable_interrupts_raii;
  return KernelAllocator.Realloc(ptr, size);
}

void *kcalloc(size_t num, size_t size) {
  DisableInterruptsRAII disable_interrupts_raii;
  return KernelAllocator.Calloc(num, size);
}

size_t GetKernelHeapUsed() { return KernelAllocator.getHeapUsed(); }
