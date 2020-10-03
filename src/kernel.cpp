#include <DescriptorTables.h>
#include <Multiboot.h>
#include <Terminal.h>
#include <Timer.h>
#include <kassert.h>
#include <kernel.h>
#include <keyboard.h>
#include <kmalloc.h>
#include <ktask.h>
#include <paging.h>
#include <panic.h>
#include <syscall.h>
#include <tests.h>
#include <vfs.h>

using terminal::Hex;
using terminal::Write;
using terminal::WriteF;

extern "C" uint8_t _start, _end;

void RunTests();

namespace {

const auto kPhysicalKernelAddrStart = reinterpret_cast<uintptr_t>(&_start);
const auto kPhysicalKernelAddrEnd = reinterpret_cast<uintptr_t>(&_end);

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
  WriteF("Upper memory (kB): {}\n", Hex(multiboot->mem_upper));
  WriteF("Kernel start:{} - end:{}\n", Hex(kPhysicalKernelAddrStart),
         Hex(kPhysicalKernelAddrEnd));
  assert(kPhysicalKernelAddrEnd - kPhysicalKernelAddrStart <= kPageSize4M &&
         "The kernel should be able to fir in a 4MB page");

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
  assert(multiboot->framebuffer_addr <= UINT32_MAX &&
         "Framebuffer cannot fit in 32 bits.");

  {
    // Initialize stuff for the kernel to work.
    InitDescriptorTables();
    InitializePaging(multiboot->mem_upper, /*pages_4K=*/true,
                     static_cast<uint32_t>(multiboot->framebuffer_addr));
    InitializeKernelHeap();
    InitTimer(50);
    InitScheduler();
    InitializeSyscalls();
    InitializeKeyboard();
  }

  {
    // Playground environment.
    RunTests();
    terminal::WriteF("#Rows: {}\n", terminal::GetNumRows());
    terminal::WriteF("#Cols: {}\n", terminal::GetNumCols());

    // Run some tasks in userspace.
    Task t2 = Task::CreateUserTask(UserspaceFunc, (void *)0xfeed);
    Task t3 = Task::CreateUserTask(UserspaceFunc2, (void *)0xfeed2);
    t2.Join();
    t3.Join();

    // Test a user program loaded as a multiboot module.
    WriteF("multiboot address: {}\n", multiboot);
    assert(reinterpret_cast<uint64_t>(multiboot) < USER_END);

    void *usercode = nullptr;
    size_t size = 0;
    {
      // Read the user program and copy it. We need to use an identity map
      // because the multiboot addresses could be within the first 4MB page of
      // memory.
      // FIXME: Instead, it might just be cleaner to memcpy() all relevant data
      // somewhere into a variable on the stack so we don't need to map address
      // space 0.
      if (terminal::UsingGraphics())
        GetKernelPageDirectory().AddPage(nullptr, nullptr, 0);

      if (multiboot->mods_count) {
        auto *moduleinfo = multiboot->getModuleBegin();
        uint8_t *modstart = reinterpret_cast<uint8_t *>(moduleinfo->mod_start);
        uint8_t *modend = reinterpret_cast<uint8_t *>(moduleinfo->mod_end);
        WriteF("vfs size: {}\n", moduleinfo->getModuleSize());
        auto vfs = vfs::ParseVFS(modstart, modend);
        vfs->Dump();

        const vfs::Node *program = vfs->getFile("user_program.bin");
        assert(program && "Could not find user_program.bin");

        const vfs::File &file = program->AsFile();
        size = file.contents.size();
        terminal::WriteF("user_program.bin is {} bytes\n", size);

        usercode = kmalloc(size);
        memcpy(usercode, file.contents.data(), size);
      }

      if (terminal::UsingGraphics())
        GetKernelPageDirectory().RemovePage(nullptr);
    }

    if (size && usercode) {
      // Actually create and run the user tasks.
      Task ut1 = Task::CreateUserTask((TaskFunc)usercode, size, (void *)0xfeed);
      Task ut2 =
          Task::CreateUserTask((TaskFunc)usercode, size, (void *)0xfeed2);
      ut1.Join();
      ut2.Join();
    } else {
      terminal::Write(
          "\n\nNOTE: Could not find the initial ramdisk (initrd). If this is "
          "running on QEMU, then either pass the image file with `-cdrom "
          "myos.iso`, or pass the ramdisk along with the kernel via `-kernel "
          "kernel -initrd initrd.vfs`.\n\n");
    }

    kfree(usercode);
  }

  DestroyScheduler();

  // Make sure all allocated memory was freed.
  WriteF("Kernel memory still in use: {} B\n", GetKernelHeapUsed());
  assert(GetKernelHeapUsed() == 0 && "Kernel heap was not cleared!");

  Write("Reached end of kernel.");
  while (1) {}
}
}
