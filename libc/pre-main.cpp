/**
 * This contains code that should run before the start of main.
 */

#include <_syscalls.h>
#include <umalloc.h>

#include <cstdio>

// NOTE: This should always have the same value as USER_START in the kernel's
// paging.h.
#define USER_START UINT32_C(0x40000000)  // 1GB

namespace {

uint32_t PageIndex4M(const void *addr) { return (uint32_t)(addr) >> 22; }
void *PageAddr4M(uint32_t page) { return (void *)(page << 22); }

// Attempt to get the next available page by getting the location of the current
// binary in memory, then getting the page after it. Note that this only works
// because binaries right now fit into a 4MB page.
void *NextPage() {
  uint32_t page = PageIndex4M((void *)&NextPage);
  return PageAddr4M(page + 1);
}

constexpr const int kExitFailure = -1;
constexpr const uint32_t kPageSize4M = 0x00400000;
constexpr const size_t kInitHeapSize = kPageSize4M;

}  // namespace

extern "C" int pre_main() {
  void *heap_start = NextPage();
  auto val = sys_map_page(heap_start);
  switch (val) {
    case MAP_UNALIGNED_ADDR:
      printf(
          "Attempting to map virtual address %p which is not aligned to "
          "page.\n",
          heap_start);
      return kExitFailure;
    case MAP_ALREADY_MAPPED:
      printf("Attempting to map virtual address %p which is already mapped.\n",
             heap_start);
      return kExitFailure;
    case MAP_OOM:
      printf("No more physical memory available!\n");
      return kExitFailure;
    default:
      printf("Allocated heap page at %p.\n", heap_start);
  }

  uint8_t *heap_bottom = (uint8_t *)heap_start;
  uint8_t *heap_top = heap_bottom + kInitHeapSize;
  user::InitializeUserHeap(heap_bottom, heap_top);
  return 0;
}
