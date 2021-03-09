#include <stdio.h>

// <setup-code>
#include <userboot.h>
#include <umalloc.h>
#include <stdint.h>
#include <_syscalls.h>

extern bool __use_debug_log;
// </setup-code>

int main() {
  // FIXME: This should be handled from libc setup.
  // <libc-setup>
  __use_debug_log = true;

  printf("=== RUNTESTS ===\n\n");

  // Allocate space for the heap on the page immediately after where this is
  // mapped.
  // TODO: Formalize this more. Setup of the heap and malloc should be the duty
  // of libc.
  void *heap_start = NextPage();
  printf("heap_start: %p\n", heap_start);
  int32_t val = sys_map_page(heap_start);
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

  // Temporary heap. This is the same strategy we use in userboot stage 1, but
  // we should really have libc establish the heap.
  uint8_t *heap_bottom = (uint8_t *)(heap_start);
  uint8_t *heap_top = heap_bottom + kInitHeapSize;

  // FIXME: This should be handled from libc setup.
  InitializeUserHeap(heap_bottom, heap_top);
  printf("Initialized userspace tests heap: %p - %p (%u bytes)\n", heap_bottom,
         heap_top, kInitHeapSize);
  // </libc-setup>

  printf("hello world\n");
  return 0;
}
