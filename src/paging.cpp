#include <Terminal.h>
#include <isr.h>
#include <knew.h>
#include <ktask.h>
#include <paging.h>

namespace {

PageDirectory KernelPageDir;
PhysicalBitmap4M PhysicalBitmap;

void HandlePageFault(registers_t *regs) {
  uint32_t faulting_addr;
  asm volatile("mov %%cr2, %0" : "=r"(faulting_addr));

  int present = regs->err_code & 0x1;
  int rw = regs->err_code & 0x2;
  int us = regs->err_code & 0x4;
  int reserved = regs->err_code & 0x8;
  int id = regs->err_code & 0x10;

  terminal::WriteF("Page fault!!! When trying to {} {} \n- IP:{}\n",
                   rw ? "write to" : "read from", terminal::Hex(faulting_addr),
                   terminal::Hex(regs->eip));
  terminal::WriteF("- The page was {}\n", present ? "present" : "not present");

  if (reserved) terminal::Write("- Reserved bit was set\n");

  if (id) terminal::Write("- Caused by an instruction fetch\n");

  terminal::WriteF("- CPU was in {}\n", us ? "user-mode" : "supervisor mode");

  if (GetCurrentTask() == GetMainKernelTask()) {
    terminal::Write("- Occurred in main kernel task.\n");
  } else {
    terminal::WriteF("- Occurred in task: {}.\n", GetCurrentTask()->getID());
  }

  DumpRegisters(regs);

  PANIC("Page fault!");
}

constexpr size_t kNumPageDirs =
    (PAGE_DIRECTORY_REGION_END - PAGE_DIRECTORY_REGION_START) / kPageDirSize;
class PageDirRegionBitmap : public toy::BitArray<kNumPageDirs> {
 public:
  void *getAndUseNextFreeRegion() {
    size_t bit;
    assert(GetFirstZero(bit) && "No free pages in the page directory region");
    setOne(bit);
    uint8_t *start = reinterpret_cast<uint8_t *>(PAGE_DIRECTORY_REGION_START);
    return start + kPageDirSize * bit;
  }

  void Reclaim(const PageDirectory *pd) {
    assert(!pd->isKernelPageDir());
    size_t addr = reinterpret_cast<size_t>(pd) - PAGE_DIRECTORY_REGION_START;
    assert(addr % kPageDirSize == 0);
    setZero(addr / kPageDirSize);
  }
};

PageDirRegionBitmap PageDirRegion;

}  // namespace

void InitializePaging(uint32_t high_mem_KB, [[maybe_unused]] bool pages_4K,
                      uint32_t framebuffer_addr) {
  terminal::Write("Initializing paging...\n");
  RegisterInterruptHandler(kPageFaultInterrupt, HandlePageFault);
  uint32_t total_mem = high_mem_KB * 1024;

  // Note: Based on how we calculate this, if we are running in QEMU, this may
  // be 32.
  // 32 4MB pages = 128 MB of total memory, which is the default memory limit
  // for QEMU.
  uint32_t num_4M_pages = total_mem / kPageSize4M + 1;
  terminal::WriteF("Total 4 MB page count: {}\n", num_4M_pages);

  // FIXME: This just happens to be the QEMU default. We should set a clear
  // value on how much memory we would like.
  assert(num_4M_pages >= 32 && "Expected at least 128 MB of memory available.");

  PhysicalBitmap.Clear();
  KernelPageDir.Clear();
  PageDirRegion.Clear();

  // The first 128 MB are not used, but the rest of the physical memory is used.
  // In actuality, we only have access to 128 MB of memory, so we can just mark
  // it as used we don't accidentally access it again.
  PhysicalBitmap.ReservePhysical(/*num_pages=*/32);  // 4 x 32 MB = 128 MB
  uint8_t flags = PG_PRESENT | PG_WRITE | PG_4MB | PG_USER;
  // uint8_t flags = PG_PRESENT | PG_WRITE | PG_4MB;

  // Pages reserved for the kernel (4MB - 12MB).
  KernelPageDir.AddPage(reinterpret_cast<void *>(KERNEL_START),
                        reinterpret_cast<void *>(KERNEL_START), flags);
  KernelPageDir.AddPage(reinterpret_cast<void *>(PAGE_DIRECTORY_REGION_START),
                        reinterpret_cast<void *>(PAGE_DIRECTORY_REGION_START),
                        flags);

  // Also need to map GFX memory here.
  PhysicalBitmap.setPageFrameFree(PageIndex4M(framebuffer_addr));

  SwitchPageDirectory(KernelPageDir);

  // Enable paging.
  // PSE is required for 4MB pages.
  asm volatile(
      "mov %%cr4, %%eax \n \
      or %1, %%eax \n \
      mov %%eax, %%cr4 \n \
      \
      mov %%cr0, %%eax \n \
      or %0, %%eax \n \
      mov %%eax, %%cr0" ::"i"(PAGING_FLAG),
      "i"(PSE_FLAG));

  // After paging is enabled, it's possible that the physical address we wrote
  // to in virtual mode may not have a page allocated for it. Continuing to
  // write to it could result in a page fault! We should immediately re-assign
  // the framebuffer to some allocated virtual memory and write to that instead.
  // FIXME: May also need to remap the text buffer (0xb8000) if not using the
  // first 4MB.
  if (terminal::UsingGraphics()) terminal::UseGraphicsTerminalVirtual();

  terminal::Write("Paging initialized!\n");
}

PageDirectory &GetKernelPageDirectory() { return KernelPageDir; }
PhysicalBitmap4M &GetPhysicalBitmap4M() { return PhysicalBitmap; }

void PageDirectory::RemovePage(void *vaddr) {
  assert(reinterpret_cast<uintptr_t>(vaddr) % kPageSize4M == 0 &&
         "Address is not 4MB aligned");
  uint32_t page = PageIndex4M(vaddr);
  PhysicalBitmap.setPageFrameFree(page);
  pd_impl_[page] = 0;
  asm volatile("invlpg %0" ::"m"(vaddr));
}

IdentityMapRAII::IdentityMapRAII(void *addr)
    : page_(PageIndex4M(reinterpret_cast<uint32_t>(addr))) {
  GetKernelPageDirectory().AddPage(addr, addr, PG_USER);
}

IdentityMapRAII::~IdentityMapRAII() {
  GetKernelPageDirectory().RemovePage(PageAddr4M(page_));
}

// Map unmapped virtual memory to available physical memory.
void PageDirectory::AddPage(void *v_addr, const void *p_addr, uint8_t flags) {
  // With 4MB pages, bits 31 through 12 are reserved, so the the physical
  // address must be 4MB aligned.
  uint32_t paddr_int = reinterpret_cast<uint32_t>(p_addr);
  assert(paddr_int % kPageSize4M == 0 &&
         "Attempting to map a page that is not 4MB aligned!");

  // Get the top 10 bits. These specify the nth page table in the page
  // directory.
  auto vaddr_int = reinterpret_cast<uint32_t>(v_addr);
  assert(vaddr_int % kPageSize4M == 0 &&
         "Attempting to map a virtual address that is not 4MB aligned");

  assert(!PhysicalBitmap.isPageFrameUsed(PageIndex4M(paddr_int)));

  uint32_t index = PageIndex4M(vaddr_int);
  uint32_t &pde = pd_impl_[index];
  assert(
      !(pde & PG_PRESENT) &&
      "The page directory entry for this virtual address is already assigned.");

  pde = (paddr_int & kPageMask4M) | (PG_PRESENT | PG_4MB | PG_WRITE | flags);

  PhysicalBitmap.setPageFrameUsed(PageIndex4M(paddr_int));

  // Invalidate page in TLB.
  asm volatile("invlpg %0" ::"m"(v_addr));
}

bool PageDirectory::isPhysicalFree(uint32_t page_index) {
  return !PhysicalBitmap.isPageFrameUsed(page_index);
}

void SwitchPageDirectory(PageDirectory &pd) {
  asm volatile("mov %0, %%cr3" ::"r"(pd.get()));
}

PageDirectory *PageDirectory::Clone() const {
  // Note that page directories created this way never need to be explicitly
  // deleted.
  auto *pd = new (PageDirRegion.getAndUseNextFreeRegion()) PageDirectory;
  memcpy(pd->get(), get(), sizeof(pd_impl_));

  // Increment refcount for all physical pages referenced at the time of this
  // clone.
  for (const uint32_t *pde = pd_impl_, *pd_end = pd_impl_ + kNumPageDirEntries;
       pde != pd_end; ++pde) {
    if (*pde & PG_PRESENT) {
      uint32_t phys_page_index = PageIndex4M(*pde);
      PhysicalBitmap.Ref(phys_page_index);
    }
  }
  return pd;
}

void PageDirectory::ReclaimPageDirRegion() const {
  // Reclaim all physical pages allocated by this page directory.
  for (const uint32_t *pde = pd_impl_, *pd_end = pd_impl_ + kNumPageDirEntries;
       pde != pd_end; ++pde) {
    if (*pde & PG_PRESENT) {
      uint32_t phys_page_index = PageIndex4M(*pde);
      PhysicalBitmap.setPageFrameFree(phys_page_index);
    }
  }
  PageDirRegion.Reclaim(this);
}

bool PageDirectory::isKernelPageDir() const { return this == &KernelPageDir; }
