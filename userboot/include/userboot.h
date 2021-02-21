#ifndef USERBOOT_H_
#define USERBOOT_H_

constexpr const uint32_t kPageSize4M = 0x00400000;

inline uint32_t PageIndex4M(const void *addr) {
  return reinterpret_cast<uint32_t>(addr) >> 22;
}
inline void *PageAddr4M(uint32_t page) {
  return reinterpret_cast<void *>(page << 22);
}

inline void *NextPage() {
  void *current_addr = __builtin_return_address(0);

  // Current page size is 4M.
  uint32_t page = PageIndex4M(current_addr);
  return PageAddr4M(page + 1);
}

#endif
