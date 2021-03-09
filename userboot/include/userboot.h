#ifndef USERBOOT_H_
#define USERBOOT_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

const uint32_t kPageSize4M = 0x00400000;

uint32_t PageIndex4M(const void *addr) { return (uint32_t)(addr) >> 22; }
void *PageAddr4M(uint32_t page) { return (void *)(page << 22); }

void *NextPage() {
  void *current_addr = __builtin_return_address(0);

  // Current page size is 4M.
  uint32_t page = PageIndex4M(current_addr);
  return PageAddr4M(page + 1);
}

const int kExitFailure = -1;
const size_t kInitHeapSize = kPageSize4M;

#ifdef __cplusplus
}  // extern "C"
#endif

#endif
