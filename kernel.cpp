#include <DescriptorTables.h>
#include <Multiboot.h>
#include <Terminal.h>
#include <Timer.h>
#include <kassert.h>
#include <kernel.h>
#include <keyboard.h>
#include <kmalloc.h>
#include <kthread.h>
#include <paging.h>
#include <panic.h>
#include <scheduler.h>
#include <syscall.h>
#include <tests.h>

using terminal::Hex;
using terminal::Write;
using terminal::WriteF;

extern "C" uint8_t _start, _end;

void RunTests();

namespace {

const auto kPhysicalKernelAddrStart = reinterpret_cast<uintptr_t>(&_start);
const auto kPhysicalKernelAddrEnd = reinterpret_cast<uintptr_t>(&_end);

void InitializeMemory(uint32_t high_mem) {
  InitializePaging(high_mem);
  InitializeKernelHeap();
}

void UserspaceFunc([[maybe_unused]] void *args) {
  syscall_terminal_write("Printing this through a syscall.\n");
  syscall_terminal_write("Printing this through a syscall2.\n");
}

void Nested() {
  syscall_terminal_write("Printing this through a syscall3.\n");
  syscall_terminal_write("Printing this through a syscall4.\n");
}

void UserspaceFunc2([[maybe_unused]] void *args) { Nested(); }

}  // namespace

extern "C" {

void kernel_main(const Multiboot *multiboot) {
  int stack_start;

  // At this point, we do not know how to print stuff yet. That is, are we
  // in VGA graphics mode or EGA text mode? We need to first identify where our
  // frame buffer is.
  if (multiboot->framebuffer_type == 1)
    terminal::UseGraphicsTerminalPhysical(multiboot, /*serial=*/true);
  else
    terminal::UseTextTerminal(/*serial=*/true);

  Write("Hello, kernel World!\n");

  WriteF("multiboot flags: {}\n", Hex(multiboot->flags));
  WriteF("Lower memory: {}\n", Hex(multiboot->mem_lower));
  WriteF("Upper memory: {}\n", Hex(multiboot->mem_upper));
  WriteF("Kernel start:{} - end:{}\n", Hex(kPhysicalKernelAddrStart),
         Hex(kPhysicalKernelAddrEnd));
  WriteF("Stack start: {}\n", &stack_start);
  WriteF("mods_count: {}\n", multiboot->mods_count);
  if (multiboot->mods_count) {
    auto *mod = multiboot->getModuleBegin();
    WriteF("module start: {}\n", Hex(mod->mod_start));
    WriteF("module end: {}\n", Hex(mod->mod_end));
    WriteF("module size: {}\n", mod->getModuleSize());

    size_t size = mod->getModuleSize();
    char data[size + 1];
    data[size] = '\0';
    memcpy(data, mod->getModuleStart(), size);
    WriteF("module contents: {}\n", data);
  }
  WriteF("framebuffer type: {}\n", Hex(multiboot->framebuffer_type));
  WriteF("physical framebuffer address: {}\n",
         Hex(multiboot->framebuffer_addr));

  {
    // Initialize stuff for the kernel to work.
    InitDescriptorTables();
    InitializeMemory(multiboot->mem_upper);
    InitTimer(50);
    init_scheduler();
    InitializeSyscalls();
    InitializeKeyboard();
  }

  {
    // Playground environment.
    RunTests();
    terminal::WriteF("#Rows: {}\n", terminal::GetNumRows());
    terminal::WriteF("#Cols: {}\n", terminal::GetNumCols());

    // Run some threads in userspace.
    thread_t *t2 = create_user_thread(UserspaceFunc, (void *)0xfeed);
    thread_t *t3 = create_user_thread(UserspaceFunc2, (void *)0xfeed2);
    thread_join(t2);
    thread_join(t3);
    kfree(t2);
    kfree(t3);

    // Test a user program loaded as a multiboot module.
    WriteF("multiboot address: {}\n", multiboot);
    assert(multiboot < reinterpret_cast<void *>(USER_END));

    {
      IdentityMapRAII identity(nullptr);

      auto *usermod = multiboot->getModuleBegin() + 1;
      WriteF("usermod start: {} - {}\n", Hex(usermod->mod_start),
             Hex(usermod->mod_end));
      WriteF("usermod size: {}\n", usermod->getModuleSize());

      size_t size = usermod->getModuleSize();
      void *data = reinterpret_cast<void *>(USER_START);
      memcpy(data, usermod->getModuleStart(), size);
      thread_t *ut1 = create_user_thread((ThreadFunc)data, (void *)0xfeed);
      thread_join(ut1);
      kfree(ut1);
    }
  }

  DestroyScheduler();

  // Make sure all allocated memory was freed.
  WriteF("Kernel memory still in use: {} B\n", GetKernelHeapUsed());
  assert(GetKernelHeapUsed() == 0 && "Kernel heap was not cleared!");

  Write("Reached end of kernel.");
  while (1) {}
}
}
