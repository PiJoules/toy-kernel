#ifndef PAGING_H_
#define PAGING_H_

#include <BitArray.h>
#include <kstdint.h>
#include <panic.h>

// Memory layout
// [0MB   - 4MB)    RESERVED
// [4MB   - 8MB)    Kernel
// [8MB   - 12MB)   Page directory region
// [12MB  - 16MB)   FREE
// [16MB  - 20MB)   GFX_MEMORY
// [32MB  - 1GB)    KERNEL_HEAP
// [1GB   - 4GB)    Flat user programs
#define KERNEL_START 0x400000
#define KERNEL_END 0x800000                     // 8MB
#define PAGE_DIRECTORY_REGION_START KERNEL_END  // 8MB
#define PAGE_DIRECTORY_REGION_END 0xC00000      // 12MB

// TODO: GFX virtual memory (which controls the screen for graphics mode) starts
// at 16 MB, but we should also clearly mark where it ends.
// NOTE: The kernel heap starts at 32 MB and ends at 1 GB.
#define GFX_MEMORY_START 0x01000000      // 16 MB
#define GFX_MEMORY_END 0x1400000         // 20 MB
#define KERN_HEAP_BEGIN 0x02000000       // 32 MB
#define KERN_HEAP_END 0x40000000         // 1 GB
#define USER_START UINT32_C(0x40000000)  // 1GB
#define USER_END UINT64_C(0x100000000)   // 2GB

#define PAGING_FLAG 0x80000000  // CR0 - bit 31
#define PSE_FLAG 0x00000010     // CR4 - bit 4
#define PG_PRESENT 0x00000001   // page directory / table
#define PG_WRITE 0x00000002
#define PG_USER 0x00000004
#define PG_4MB 0x00000080

constexpr uint8_t kPageFaultInterrupt = 14;

constexpr uint32_t kPageMask4M = ~UINT32_C(0x3FFFFF);
constexpr uint32_t kPageSize4M = 0x00400000;
constexpr uint32_t kRamAs4MPages = 0x400;  // Tofal RAM = 1024 x 4 MB = 4 GB
constexpr uint32_t kRamAs4KPages =
    0x100000;  // Tofal RAM = 0x100000 x 4 KB = 4 GB

constexpr uint32_t PageIndex4M(uint32_t addr) { return addr >> 22; }
inline uint32_t PageIndex4M(void *addr) {
  return reinterpret_cast<uint32_t>(addr) >> 22;
}
inline void *PageAddr4M(uint32_t page) {
  return reinterpret_cast<void *>(page << 22);
}

constexpr size_t kNumPageDirEntries = 1024;
constexpr size_t kPageDirAlignment =
    4096;  // Page directories must be 4 kB aligned.
constexpr size_t kPageDirSize = 4096;

// FIXME: Eventually remove the `framebuffer_addr` argument.
void InitializePaging(uint32_t high_mem, bool pages_4K,
                      uint32_t framebuffer_addr);

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
//
// FIXME: The amount of physical memory we have should not be assumed here.
// Ideally, we would get this upper bound from multiboot (although that
// might not always be true; see
// https://stackoverflow.com/a/45549699/2775471). For multiboot
// specifically, using mem_upper will only return the end of the first
// contiguous chunk of memory up until the first memory hole. We should
// eventually switch to using the mmap_* fields which should be able to map
// all memory regions
// (https://wiki.osdev.org/Detecting_Memory_(x86)#Memory_Map_Via_GRUB).
class PhysicalBitmap4M : public toy::BitArray<kRamAs4MPages> {
 public:
  void setPageFrameUsed(size_t page_index) {
    Ref(page_index);
    setOne(page_index);
  }

  void setPageFrameFree(size_t page_index) {
    Unref(page_index);
    if (refs_[page_index] == 0) setZero(page_index);
  }

  bool isPageFrameUsed(size_t page_index) const { return isSet(page_index); }

  /**
   * Specify the number of pages of physical memory available.
   */
  void ReservePhysical(size_t num_pages) {
    Reserve(num_pages);
    for (; num_pages < kRamAs4MPages; ++num_pages) Ref(num_pages);
  }

  uint8_t *NextFreePhysicalPage(size_t start = 0) const {
    size_t page_4MB;
    assert(GetFirstZero(page_4MB, start) && "Memory is full!");
    return reinterpret_cast<uint8_t *>(page_4MB * kPageSize4M);
  }

  void Ref(size_t page_index) { ++(refs_[page_index]); }

  void Unref(size_t page_index) {
    auto &ref = refs_[page_index];
    assert(ref && "Attempting to unref a page that has no references");
    --ref;
  }

  auto getRefs(size_t page_index) const { return refs_[page_index]; }

 private:
  // Whenever we clone a page directory, we also duplicate references to page
  // indexes for physical memory. If we destroy a page directory that was the
  // forst to map a specific physical page, that phsyical page should be made
  // available again. In order to keep track of the number of references each
  // physical page has, we can just ref-count the number of page directories
  // that reference it.
  //
  // Note that (ideally) we would have one reference/cloned page directory per
  // new process, which is when a new page directory is required for a new
  // address space. But technically, we can have a new reference every time we
  // call PageDirectory::Clone(). Therefore, the number of references to a
  // specific physical page can technically be infinite, but at the bare
  // minimum, the refcount should be able to hold the maximum number of page
  // directories we allow (that is, we have the maximum number of page
  // directories available, each refering to the same page index).
  //
  // The number of refs we have should be the number of 4MB pages we have.
  //
  // FIXME: Should this be atomic?
  uint16_t refs_[kRamAs4MPages];
  static_assert(
      ipow2<uint32_t>(sizeof(*refs_) * CHAR_BIT) >= kRamAs4MPages,
      "Expected to fit at least one reference for each possible 4MB page.");
};

// FIXME: Might be cleaner to just have 2 subclasses: one for 4K and one for 4M.
class PageDirectory {
 public:
  uint32_t *get() { return pd_impl_; }
  const uint32_t *get() const { return pd_impl_; }

  // Map unmapped virtual memory to available physical memory.
  //
  // TODO: Might be useful to have a method that does mapping to previously
  // mapped physical memory. This could be useful for shared memory.
  void AddPage(void *v_addr, const void *p_addr, uint8_t flags);

  void RemovePage(void *vaddr);

  void Clear() { memset(pd_impl_, 0, sizeof(pd_impl_)); }

  PageDirectory *Clone() const;
  bool isKernelPageDir() const;
  void ReclaimPageDirRegion() const;
  static bool isPhysicalFree(uint32_t page_index);
  bool isVirtualMapped(void *v_addr) const;

 private:
  alignas(kPageDirAlignment) uint32_t pd_impl_[kNumPageDirEntries];
};

static_assert(sizeof(PageDirectory) == kPageDirSize,
              "The page directory no longer fits in 4KB");
static_assert(alignof(PageDirectory) == kPageDirAlignment,
              "The page directory must be 4KB aligned");

PageDirectory &GetKernelPageDirectory();
PhysicalBitmap4M &GetPhysicalBitmap4M();
void SwitchPageDirectory(PageDirectory &pd);

struct IdentityMapRAII {
  IdentityMapRAII(void *addr);
  ~IdentityMapRAII();

 private:
  uint32_t page_;
};

#endif
