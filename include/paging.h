#ifndef PAGING_H_
#define PAGING_H_

#include <kmalloc.h>
#include <kstdint.h>

#define SET_PAGEFRAME_USED(bitmap, pageIndex) \
  bitmap[pageIndex / 8] |= (UINT32_C(1) << (pageIndex) % 8)
#define SET_PAGEFRAME_FREE(bitmap, pageIndex) \
  bitmap[pageIndex / 8] &= ~(UINT32_C(1) << (pageIndex) % 8)

// Memory layout
// [0MB   - 4MB)    Flat user programs
// [4MB   - 8MB)    Kernel
// [8MB   - 16MB)   FREE
// [16MB  - 20MB)   GFX_MEMORY
// [32MB  - 1GB)    KERNEL_HEAP
#define USER_START 0x0
#define USER_END 0x400000  // 4 MB
#define KERNEL_START USER_END
#define KERNEL_END 0x800000  // 8MB

// TODO: GFX virtual memory (which controls the screen for graphics mode) starts
// at 16 MB, but we should also clearly mark where it ends.
// NOTE: The kernel heap starts at 32 MB and ends at 1 GB.
#define GFX_MEMORY_START 0x01000000  // 16 MB
#define GFX_MEMORY_END 0x1400000     // 20 MB
#define KERN_HEAP_BEGIN 0x02000000   // 32 MB
#define KERN_HEAP_END 0x40000000     // 1 gb

#define PAGING_FLAG 0x80000000  // CR0 - bit 31
#define PSE_FLAG 0x00000010     // CR4 - bit 4
#define PG_PRESENT 0x00000001   // page directory / table
#define PG_WRITE 0x00000002
#define PG_USER 0x00000004
#define PG_4MB 0x00000080

constexpr unsigned kPageSize4M = 0x00400000;
constexpr unsigned kRamAs4MPages = 1024;  // Tofal RAM = 1024 x 4 MB = 4 GB
extern uint8_t PhysicalPageFrameBitmap[kRamAs4MPages / 8];

constexpr uint32_t PageIndex4M(uint32_t addr) { return addr >> 22; }
inline uint32_t PageIndex4M(void *addr) {
  return reinterpret_cast<uint32_t>(addr) >> 22;
}
inline void *PageAddr4M(uint32_t page) {
  return reinterpret_cast<void *>(page << 22);
}

constexpr size_t kNumPageDirEntries = kRamAs4MPages;

void InitializePaging(uint32_t high_mem);

class PageDirectory {
 public:
  uint32_t *get() { return pd_impl_; }

  // Map virtual memory to physical memory.
  void AddPage(void *v_addr, const void *p_addr, uint8_t flags);

  void RemovePage(void *vaddr);
  uint8_t *NextFreePhysicalPage() const;

  /**
   * Specify uthe number of pages of physical memory available.
   */
  void ReservePhysical(size_t num_pages) {
    assert(num_pages < kNumPageDirEntries);
    for (size_t page = num_pages; page < kNumPageDirEntries; ++page)
      setPageFrameUsed(page);
  }

  void Clear() {
    memset(pd_impl_, 0, sizeof(pd_impl_));
    memset(physical_bitmap_, 0, sizeof(physical_bitmap_));
  }

 private:
  void setPageFrameUsed(size_t page_index) {
    assert(page_index < kNumPageDirEntries);
    physical_bitmap_[page_index / 8] |= (UINT32_C(1) << (page_index % 8));
  }

  void setPageFrameFree(size_t page_index) {
    assert(page_index < kNumPageDirEntries);
    physical_bitmap_[page_index / 8] &= ~(UINT32_C(1) << (page_index % 8));
  }

  bool isPageFrameUsed(size_t page_index) const {
    return physical_bitmap_[page_index / 8] & (UINT8_C(1) << (page_index % 8));
  }

  alignas(4096) uint32_t pd_impl_[kNumPageDirEntries];

  // NOTE: WE ARE USING 4MB PAGES!
  // Bitmap used for keeping track of which 4MB chunks of physical memory are
  // used. The entire array represents the entire 4GB range of virtual memory.
  //
  // PhysicalPageFrameBitmap[0] & 1 represents the first 4 MB [0 MB - 4 MB),
  // PhysicalPageFrameBitmap[0] & 2 represents the second 4 MB [4 MB - 8 MB),
  // PhysicalPageFrameBitmap[0] & 4 represents the third 4 MB, [8 MB - 12 MB),
  // ...
  // PhysicalPageFrameBitmap[0] & 128 represents the eigth 4 MB, [28 MB - 32
  // MB), PhysicalPageFrameBitmap[1] & 1 represents the ninth 4 MB, [32 MB - 36
  // MB),
  // ...
  uint8_t physical_bitmap_[kRamAs4MPages / 8];
};

PageDirectory &GetKernelPageDirectory();
PageDirectory &GetCurrentPageDirectory();
void SwitchPageDirectory(PageDirectory &pd);

struct IdentityMapRAII {
  IdentityMapRAII(void *addr);
  ~IdentityMapRAII();

 private:
  uint32_t page_;
};

#endif
