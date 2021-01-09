#include <DescriptorTables.h>
#include <Multiboot.h>
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

extern "C" uint8_t _start, _end;

namespace {

const auto kPhysicalKernelAddrStart = reinterpret_cast<uintptr_t>(&_start);
const auto kPhysicalKernelAddrEnd = reinterpret_cast<uintptr_t>(&_end);

Task RunFlatUserBinary(const vfs::Directory &vfs, const toy::String &filename,
                       void *arg = nullptr) {
  const vfs::Node *program = vfs.getFile(filename);
  assert(program && "Could not find binary");

  const vfs::File &file = program->AsFile();
  size_t size = file.contents.size();
  DebugPrint("{} is {} bytes\n", filename.c_str(), size);

  // Actually create and run the user tasks.
  return Task::CreateUserTask((TaskFunc)file.contents.data(), size, arg);
}

/**
 * Setup the kernel to allow full functionality by the time we leave this
 * function. Set the number of modules provided in the multiboot. If there is at
 * least one module, set the start and end of the first module in mod_start and
 * mod_end. In this case, the mod_start pointer should be freed by the caller.
 * If there are no modules, then these values are not set and unchanged
 * from their initial values. Only the first is needed since that can carry the
 * initial ramdisk.
 */
void KernelSetup(const Multiboot *multiboot, size_t *num_mods,
                 uint8_t **mod_start, uint8_t **mod_end) {
  DebugPrint("multiboot flags: {}\n", Hex(multiboot->flags));
  DebugPrint("Lower memory: {}\n", Hex(multiboot->mem_lower));
  DebugPrint("Upper memory (kB): {}\n", Hex(multiboot->mem_upper));
  DebugPrint("Kernel start:{} - end:{}\n", Hex(kPhysicalKernelAddrStart),
             Hex(kPhysicalKernelAddrEnd));
  assert(kPhysicalKernelAddrEnd - kPhysicalKernelAddrStart <= kPageSize4M &&
         "The kernel should be able to fir in a 4MB page");

  DebugPrint("mods_count: {}\n", multiboot->mods_count);
  if (multiboot->mods_count) {
    auto *mod = multiboot->getModuleBegin();
    DebugPrint("module start: {}\n", Hex(mod->mod_start));
    DebugPrint("module end: {}\n", Hex(mod->mod_end));
    DebugPrint("module size: {}\n", mod->getModuleSize());
  }
  DebugPrint("framebuffer type: {}\n", Hex(multiboot->framebuffer_type));
  DebugPrint("physical framebuffer address: {}\n",
             Hex(multiboot->framebuffer_addr));
  assert(multiboot->framebuffer_addr <= UINT32_MAX &&
         "Framebuffer cannot fit in 32 bits.");

  *num_mods = multiboot->mods_count;

  // Test a user program loaded as a multiboot module.
  DebugPrint("multiboot address: {}\n", multiboot);
  assert(reinterpret_cast<uint64_t>(multiboot) < USER_END);

  uint32_t mem_upper = multiboot->mem_upper;

  // Initialize stuff for the kernel to work.
  InitDescriptorTables();
  DebugPrint("Descriptor tables initialized.\n");
  InitializePaging(mem_upper, /*pages_4K=*/true);
  DebugPrint("Paging initialized.\n");
  InitializeKernelHeap();
  DebugPrint("Heap initialized.\n");
  InitTimer(50);
  DebugPrint("Timer initialized.\n");
  InitScheduler();
  DebugPrint("Scheduler initialized.\n");
  InitializeSyscalls();
  DebugPrint("Syscalls initialized.\n");

  // FIXME: The keyboard driver should eventually be moved to userspace also.
  InitializeKeyboard();
  DebugPrint("Keyboard initialized.\n");

  if (*num_mods) {
    // NOTE: After we initialize paging, we may not be able to access all data
    // pointed to by multiboot located in the first page of memory. All that
    // data should be copied over now to some local variable if we want to
    // access it after paging is enabled.
    GetKernelPageDirectory().AddPage(nullptr, nullptr, 0,
                                     /*allow_physical_reuse=*/true);

    auto *moduleinfo = multiboot->getModuleBegin();
    size_t modsize = moduleinfo->getModuleSize();

    *mod_start = toy::kmalloc<uint8_t>(modsize);
    memcpy(*mod_start, reinterpret_cast<uint8_t *>(moduleinfo->mod_start),
           modsize);
    *mod_end = *mod_start + modsize;

    GetKernelPageDirectory().RemovePage(nullptr);
  }

  DebugPrint("Kernel setup complete.\n");
}

static void CheckGPFTriggerred(X86Registers *regs) {
  assert(regs->int_no == kGeneralProtectionFault);
  exit_this_task();
}

/**
 * Playground environment now that the kernel is setup.
 *
 * FIXME: Ideally most of this would be moved into userspace and we should be
 * able to call them from there. Running user tests and loading the vfs should
 * be handled from userspace.
 */
void KernelLoadPrograms(const vfs::Directory *vfs) {
  // FIXME: test_user_program.bin is a user binary that is run for checking that
  // user programs have their own address spaces. Ideally, this would be
  // contained in its own test rather than hardcoding it here.
  DebugPrint("Checking userspace tasks...\n");

  Task t1 = RunFlatUserBinary(*vfs, "test_user_program.bin");
  DebugPrint("Launched task {}\n", t1.getID());
  Task t2 = RunFlatUserBinary(*vfs, "test_user_program.bin");
  DebugPrint("Launched task {}\n", t2.getID());
  t1.Join();
  t2.Join();
  assert(t1.getPageDirectory().get() != t2.getPageDirectory().get() &&
         "Expected user tasks to have unique address spaces.");

  {
    // Test that I/O instructions are not triggerred in userspace.
    auto old_handler = GetInterruptHandler(kGeneralProtectionFault);
    RegisterInterruptHandler(kGeneralProtectionFault, CheckGPFTriggerred);
    Task io_priv_test = RunFlatUserBinary(*vfs, "test_user_io_privilege");
    io_priv_test.Join();

    DebugPrint(
        "General protection fault is triggerred on an I/O instruction in "
        "userspace!\n");
    RegisterInterruptHandler(kGeneralProtectionFault, old_handler);
  }

  if (vfs->hasFile("userboot")) {
    DebugPrint("Launching userboot...\n");
    Task userboot = RunFlatUserBinary(*vfs, "userboot");
    userboot.Join();
  }
}

void KernelEnd() {
  DestroyScheduler();

  // Make sure all allocated memory was freed.
  DebugPrint("Kernel memory still in use: {} B\n", GetKernelHeapUsed());
  assert(GetKernelHeapUsed() == 0 && "Kernel heap was not cleared!");

  DebugPrint("Reached end of kernel.\n");
  while (1) {
    char c = serial::Read();
    serial::Put(c);
  }
}

}  // namespace

extern "C" void kernel_main(const Multiboot *multiboot) {
  int stack_start;
  DebugPrint("Hello, kernel World!\n");
  DebugPrint("Kernel stack start: {}\n", &stack_start);

  size_t num_mods;
  uint8_t *mod_start, *mod_end;
  KernelSetup(multiboot, &num_mods, &mod_start, &mod_end);

  //-------------------------------------------------------------------------
  // After this point, we can load user programs or do whatever in kernel
  // space.
  //-------------------------------------------------------------------------

  // FIXME: Find a way to run kernel-specific tests elsewhere instead of at
  // startup. Perhaps the tests should only be included for debug builds?
  RunTests();

  DebugPrint("# of multiboot modules: {}\n", num_mods);
  assert(num_mods <= 1 &&
         "Expected at most one multiboot module, which is the optional initial "
         "ramdisk");

  if (num_mods) {
    DebugPrint("vfs size: {} bytes\n", mod_end - mod_start);

    toy::Unique<vfs::Directory> vfs;
    vfs = vfs::ParseVFS(mod_start, mod_end);
    kfree(mod_start);

    KernelLoadPrograms(vfs.get());
  } else {
    DebugPrint(
        "\n\nNOTE: Could not find the initial ramdisk (initrd). If this is "
        "running on QEMU, then either pass the image file with `-cdrom "
        "myos.iso`, or pass the ramdisk along with the kernel via `-kernel "
        "kernel -initrd initrd.vfs`.\n\n");
  }

  KernelEnd();
}
