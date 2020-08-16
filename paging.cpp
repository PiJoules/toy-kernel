#include <Terminal.h>
#include <isr.h>
#include <kthread.h>
#include <paging.h>
#include <panic.h>

namespace {

PageDirectory KernelPageDir;

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

  if (GetCurrentThread() == GetMainKernelThread()) {
    terminal::Write("- Occurred in main kernel thread.\n");
  } else {
    terminal::WriteF("- Occurred in thread: {}.\n", GetCurrentThread()->id);
  }

  DumpRegisters(regs);

  PANIC("Page fault!");
}

}  // namespace

void InitializePaging(uint32_t high_mem_KB, bool pages_4K) {
  terminal::Write("Initializing paging...\n");
  RegisterInterruptHandler(14, HandlePageFault);
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

  if (pages_4K)
    KernelPageDir.Init4KPages();
  else
    KernelPageDir.Init4MPages();
  KernelPageDir.Clear();

  // The first 128 MB are not used, but the rest of the physical memory is used.
  // In actuality, we only have access to 128 MB of memory, so we can just mark
  // it as used we don't accidentally access it again.
  KernelPageDir.ReservePhysical(/*num_pages=*/32);  // 4 x 32 MB = 128 MB
  uint8_t flags = PG_PRESENT | PG_WRITE | PG_4MB | PG_USER;
  // uint8_t flags = PG_PRESENT | PG_WRITE | PG_4MB;

  // Pages reserved for the kernel (4MB - 20MB).
  for (auto i = 1; i < 4; ++i) {
    void *addr = reinterpret_cast<void *>(i * kPageSize4M);
    KernelPageDir.AddPage(addr, addr, flags);
  }

  // FIXME: Also need to map GFX memory here.

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

void PageDirectory::RemovePage(void *vaddr) {
  assert(reinterpret_cast<uintptr_t>(vaddr) % kPageSize4M == 0 &&
         "Address is not 4MB aligned");
  uint32_t page = PageIndex4M(vaddr);
  setPageFrameFree(page);
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

// Map virtual memory to physical memory.
void PageDirectory::AddPage(void *v_addr, const void *p_addr, uint8_t flags) {
  // With 4MB pages, bits 21 through 12 are reserved, so the the physical
  // address must be 4MB aligned.
  uint32_t paddr_int = reinterpret_cast<uint32_t>(p_addr);
  assert(paddr_int % kPageSize4M == 0 &&
         "Attempting to map a page that is not 4MB aligned!");

  // Get the top 10 bits. These specify the nth page table in the page
  // directory.
  auto vaddr_int = reinterpret_cast<uint32_t>(v_addr);
  assert(vaddr_int % kPageSize4M == 0 &&
         "Attempting to map a virtual address that is not 4MB aligned");

  // TODO: Assert that we are not mapping to an already used physical page.
  // assert(!isPageFrameUsed(PageIndex4M(paddr_int)));

  uint32_t index = PageIndex4M(vaddr_int);
  uint32_t &page_table = pd_impl_[index];
  assert((page_table & PG_PRESENT) != PG_PRESENT &&
         "The page table entry for this virtual address is already assigned.");

  page_table = paddr_int | (PG_PRESENT | PG_4MB | PG_WRITE | flags);

  setPageFrameUsed(PageIndex4M(paddr_int));

  // Invalidate page in TLB.
  asm volatile("invlpg %0" ::"m"(v_addr));
}

uint8_t *PageDirectory::NextFreePhysicalPage() const {
  for (unsigned byte = 0; byte < getBitmapSize(); ++byte) {
    // Shortcut for checking if an 4MB chunks are available in this 32 MB chunk.
    if (getBitmap()[byte] != 0xFF) {
      // Check for a free 4MB chunk.
      for (unsigned bit = 0; bit < CHAR_BIT; ++bit) {
        // FIXME: We should not ignore the very first page. We only do this so
        // we don't accidentally write over user memory, but we should be
        // creating a separate page directory for that.
        if (byte == 0 && bit == 0) continue;

        if (!(getBitmap()[byte] & (UINT8_C(1) << bit))) {
          uint32_t page_4MB = CHAR_BIT * byte + bit;
          return reinterpret_cast<uint8_t *>(page_4MB * kPageSize4M);
        }
      }
    }
  }
  PANIC("Memory is full!");
  return nullptr;
}

void SwitchPageDirectory(PageDirectory &pd) {
  asm volatile("mov %0, %%cr3" ::"r"(pd.get()));
}
