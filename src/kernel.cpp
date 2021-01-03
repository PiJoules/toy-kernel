#include <DescriptorTables.h>
#include <Multiboot.h>
#include <Terminal.h>
#include <Timer.h>
#include <io.h>
#include <kassert.h>
#include <kernel.h>
#include <keyboard.h>
#include <kmalloc.h>
#include <ktask.h>
#include <paging.h>
#include <panic.h>
#include <serial.h>
#include <syscall.h>
#include <tests.h>
#include <vfs.h>

using print::Hex;
// using terminal::Write;
// using terminal::WriteF;

extern "C" uint8_t _start, _end;

void RunTests();

namespace {

const auto kPhysicalKernelAddrStart = reinterpret_cast<uintptr_t>(&_start);
const auto kPhysicalKernelAddrEnd = reinterpret_cast<uintptr_t>(&_end);

/**
 * Printing to serial.
 */
template <typename... Rest>
void DebugPrint(const char *str, Rest... rest) {
  print::Print(serial::Put, str, rest...);
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

Task RunUserProgram(const vfs::Directory &vfs, const toy::String &filename,
                    void *&usercode, void *arg = nullptr) {
  const vfs::Node *program = vfs.getFile(filename);
  assert(program && "Could not find user_program.bin");

  const vfs::File &file = program->AsFile();
  size_t size = file.contents.size();
  DebugPrint("user_program.bin is {} bytes\n", size);

  usercode = kmalloc(size);
  memcpy(usercode, file.contents.data(), size);

  // Actually create and run the user tasks.
  return Task::CreateUserTask((TaskFunc)usercode, size, arg);
}

void KernelPostInit(size_t num_mods, const vfs::Directory *vfs);
void KernelEnd();

}  // namespace

extern "C" void kernel_main(const Multiboot *multiboot) {
  int stack_start;

  // At this point, we do not know how to print stuff yet. That is, are we
  // in VGA graphics mode or EGA text mode? We need to first identify where our
  // frame buffer is.
  if (multiboot->framebuffer_type == 1)
    terminal::UseGraphicsTerminalPhysical(multiboot);
  else
    terminal::UseTextTerminal();

  DebugPrint("Hello, kernel World!\n");

  DebugPrint("multiboot flags: {}\n", Hex(multiboot->flags));
  DebugPrint("Lower memory: {}\n", Hex(multiboot->mem_lower));
  DebugPrint("Upper memory (kB): {}\n", Hex(multiboot->mem_upper));
  DebugPrint("Kernel start:{} - end:{}\n", Hex(kPhysicalKernelAddrStart),
             Hex(kPhysicalKernelAddrEnd));
  assert(kPhysicalKernelAddrEnd - kPhysicalKernelAddrStart <= kPageSize4M &&
         "The kernel should be able to fir in a 4MB page");

  DebugPrint("Stack start: {}\n", &stack_start);
  DebugPrint("mods_count: {}\n", multiboot->mods_count);
  if (multiboot->mods_count) {
    auto *mod = multiboot->getModuleBegin();
    DebugPrint("module start: {}\n", Hex(mod->mod_start));
    DebugPrint("module end: {}\n", Hex(mod->mod_end));
    DebugPrint("module size: {}\n", mod->getModuleSize());

    size_t size = mod->getModuleSize();
    char data[size + 1];
    data[size] = '\0';
    memcpy(data, mod->getModuleStart(), size);
    DebugPrint("module contents: {}\n", data);
  }
  DebugPrint("framebuffer type: {}\n", Hex(multiboot->framebuffer_type));
  DebugPrint("physical framebuffer address: {}\n",
             Hex(multiboot->framebuffer_addr));
  assert(multiboot->framebuffer_addr <= UINT32_MAX &&
         "Framebuffer cannot fit in 32 bits.");

  // Test a user program loaded as a multiboot module.
  DebugPrint("multiboot address: {}\n", multiboot);
  assert(reinterpret_cast<uint64_t>(multiboot) < USER_END);

  uint32_t mem_upper = multiboot->mem_upper;
  uint32_t framebuffer_addr =
      static_cast<uint32_t>(multiboot->framebuffer_addr);

  {
    // Initialize stuff for the kernel to work.
    InitDescriptorTables();
    InitializePaging(mem_upper, /*pages_4K=*/true, framebuffer_addr);
    InitializeKernelHeap();
    InitTimer(50);
    InitScheduler();
    InitializeSyscalls();
    InitializeKeyboard();
  }

  // NOTE: After we initialize paging, we may not be able to access all data
  // pointed to by multiboot located in the first page of memory. All that data
  // should be copied over now to some local variable if we want to access it
  // after paging is enabled.
  // FIXME: We only need this check because the buffer when using textual mode
  // is identity mapped, but we should map it elsewhere. Ideally, we'd have the
  // first page unmapped by default.
  if (terminal::UsingGraphics())
    GetKernelPageDirectory().AddPage(nullptr, nullptr, 0,
                                     /*allow_physical_reuse=*/true);

  size_t num_mods = multiboot->mods_count;
  assert(num_mods < 2 &&
         "Expected at least the kernel to be loaded with an optional initial "
         "ramdisk");

  {
    toy::Unique<vfs::Directory> vfs;
    if (num_mods) {
      auto *moduleinfo = multiboot->getModuleBegin();
      uint8_t *modstart = reinterpret_cast<uint8_t *>(moduleinfo->mod_start);
      uint8_t *modend = reinterpret_cast<uint8_t *>(moduleinfo->mod_end);
      DebugPrint("vfs size: {}\n", moduleinfo->getModuleSize());
      vfs = vfs::ParseVFS(modstart, modend);
    }

    if (terminal::UsingGraphics()) GetKernelPageDirectory().RemovePage(nullptr);

    KernelPostInit(num_mods, vfs.get());
  }
  KernelEnd();
}

namespace {

void KernelPostInit(size_t num_mods, const vfs::Directory *vfs) {
  // Playground environment.
  RunTests();
  terminal::WriteF("#Rows: {}\n", terminal::GetNumRows());
  terminal::WriteF("#Cols: {}\n", terminal::GetNumCols());

  // Run some tasks in userspace.
  Task t2 = Task::CreateUserTask(UserspaceFunc, (void *)0xfeed);
  Task t3 = Task::CreateUserTask(UserspaceFunc2, (void *)0xfeed2);
  t2.Join();
  t3.Join();

  if (num_mods) {
    void *usercode, *usercode2;
    Task t = RunUserProgram(*vfs, "user_program.bin", usercode);
    Task t2 = RunUserProgram(*vfs, "user_program.bin", usercode2);
    t.Join();
    t2.Join();

    kfree(usercode);
    kfree(usercode2);
  } else {
    terminal::Write(
        "\n\nNOTE: Could not find the initial ramdisk (initrd). If this is "
        "running on QEMU, then either pass the image file with `-cdrom "
        "myos.iso`, or pass the ramdisk along with the kernel via `-kernel "
        "kernel -initrd initrd.vfs`.\n\n");
  }
}

void KernelEnd() {
  DestroyScheduler();

  // Make sure all allocated memory was freed.
  DebugPrint("Kernel memory still in use: {} B\n", GetKernelHeapUsed());
  assert(GetKernelHeapUsed() == 0 && "Kernel heap was not cleared!");

  DebugPrint("Reached end of kernel.");
  uint8_t scancode = Read8(0x60);
  DebugPrint("last scancode: {}\n", scancode);
  while (1) {
    char c = serial::Read();
    serial::Put(c);
  }
}

}  // namespace
