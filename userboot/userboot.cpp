#include <_syscalls.h>
#include <elf.h>
#include <stdio.h>
#include <umalloc.h>
#include <userboot.h>
#include <vfs.h>

extern bool __use_debug_log;

namespace {

// NOTE: This should always have the same value as USER_START in the kernel's
// paging.h.
#define USER_START UINT32_C(0x40000000)  // 1GB

void LoadElfProgram(const uint8_t *elf_data, void *arg) {
  const auto *hdr = reinterpret_cast<const Elf32_Ehdr *>(elf_data);
  assert(IsValidElf(hdr) && "Invalid elf program");

  const auto *p_entry =
      reinterpret_cast<const Elf32_Phdr *>(elf_data + hdr->e_phoff);
  for (int i = 0; i < hdr->e_phnum; ++i, ++p_entry) {
    if (p_entry->p_type != PT_LOAD) continue;

    uint32_t v_begin = p_entry->p_vaddr;
    uint32_t v_end = v_begin + p_entry->p_memsz;
    if (v_begin < USER_START) {
      printf("Warning: Skipped to load %d bytes to %x\n", p_entry->p_filesz,
             v_begin);
      continue;
    }

    // Get the first one that has executable code.
    if (!(p_entry->p_flags & PF_X)) continue;

    printf("v_begin: %x\n", v_begin);
    printf("offset to start of elf: %x\n", p_entry->p_offset);
    printf("size: %u\n", p_entry->p_filesz);

    const uint8_t *start = elf_data + p_entry->p_offset;
    printf("eip[0]: %x\n", start[0]);

    sys::Handle handle =
        sys::CreateTask(elf_data + p_entry->p_offset, p_entry->p_filesz, arg);
    printf("Created handle %u for elf file\n", handle);
    sys::DestroyTask(handle);

    // Stop after loading only the first one.
    return;
  }
}

void RunFlatUserBinary(const vfs::Directory &vfs, const std::string &filename,
                       void *arg = nullptr) {
  const vfs::Node *program = vfs.getFile(filename);
  assert(program && "Could not find binary");

  const vfs::File &file = program->AsFile();
  size_t size = file.contents.size();
  printf("%s is %u bytes\n", filename.c_str(), size);

  // Actually create and run the user tasks.
  sys::Handle handle = sys::CreateTask(file.contents.data(), size, arg);
  printf("Created %u\n", handle);
  sys::DestroyTask(handle);
}

}  // namespace

extern "C" int __user_main(void *stack) {
  __use_debug_log = true;

  printf("\n=== USERBOOT STAGE 1 ===\n\n");
  printf(
      "  This program is meant to simply run Userboot Stage 2, which \n"
      "  contains the rest of the userboot code, but can be compiled as a \n"
      "  \"mostly\" normal ELF binary. Compiling as an ELF binary offers \n"
      "  better debugability and building with fewer \"special\" flags.\n\n");

  // This is the dummy argument we expect from the kernel.
  printf("stack: %p\n", stack);
  void *arg = ((void **)stack)[0];
  printf("arg (stack[0]): %p\n", arg);
  size_t vfs_size;
  memcpy(&vfs_size, arg, sizeof(vfs_size));
  printf("vfs size: %u\n", vfs_size);
  uint8_t *vfs_data = (uint8_t *)arg + sizeof(vfs_size);
  printf("vfs start: %p\n", vfs_data);

  // This is space we allocate exclusively for using as a heap during bringup.
  // We use this pre-allocated space as a brief substitute for an actual heap.
  // Note that we create the heap here instead of in bss so we can save some
  // space in this binary when storing it into the initial ramdisk.
  alignas(16) uint8_t kHeap[kHeapSize];
  memset(kHeap, 0, kHeapSize);
  uint8_t *kHeapBottom = kHeap;
  uint8_t *kHeapTop = &kHeap[kHeapSize];

  printf("heap_bottom: %p\n", kHeapBottom);
  printf("heap_top: %p\n", kHeapTop);

  printf("heap space: %u\n", kHeapSize);
  assert(kHeapSize > vfs_size &&
         "The heap allocation is not large enough to hold the vfs. More space "
         "should be allocated for bootstrapping.");

  user::InitializeUserHeap(kHeapBottom, kHeapTop);
  std::unique_ptr<vfs::Directory> vfs;
  uint32_t entry_offset;
  vfs = vfs::ParseVFS(vfs_data, vfs_data + vfs_size, entry_offset);

  printf("entry point offset: %u\n", entry_offset);
  printf("vfs:\n");
  vfs->Dump();

  // Check starting a user task via syscall.
  printf("arg 3: %p\n", arg);
  void *arg_saved = arg;
  printf("Trying test_user_program.bin ...\n");
  RunFlatUserBinary(*vfs, "test_user_program.bin");
  printf("Finished test_user_program.bin.\n");
  printf("arg 4: %p\n", arg);
  printf("arg_saved: %p\n", arg_saved);
  assert(arg_saved == arg);

  printf("\nWelcome! Type \"help\" for commands\n");

  if (const vfs::Node *file = vfs->getFile("userboot-stage2")) {
    LoadElfProgram(file->AsFile().contents.data(), arg);
  } else {
    printf(
        "ERROR: Missing \"userboot-stage2\" binary. Exiting Userboot Stage 1 "
        "now.\n");
  }

  return 0;
}
