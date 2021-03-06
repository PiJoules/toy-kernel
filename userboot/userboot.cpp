#include <_syscalls.h>
#include <elf.h>
#include <umalloc.h>
#include <userboot.h>
#include <vfs.h>

#include <cstdio>

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

constexpr const int kExitFailure = -1;

}  // namespace

extern "C" uint8_t __binary_start, __binary_end;

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
  size_t initrd_size;
  memcpy(&initrd_size, arg, sizeof(initrd_size));
  printf("initrd size: %u\n", initrd_size);
  uint8_t *initrd_data = (uint8_t *)arg + sizeof(initrd_size);
  printf("initrd start: %p\n", initrd_data);

  // Allocate space for the heap on the page immediately after where this is
  // mapped.
  // TODO: Formalize this more. Setup of the heap and malloc should be the duty
  // of libc.
  void *heap_start = NextPage();
  auto val = sys_map_page(heap_start);
  switch (val) {
    case MAP_UNALIGNED_ADDR:
      printf(
          "Attempting to map virtual address %p which is not aligned to "
          "page.\n",
          heap_start);
      return kExitFailure;
    case MAP_ALREADY_MAPPED:
      printf("Attempting to map virtual address %p which is already mapped.\n",
             heap_start);
      return kExitFailure;
    case MAP_OOM:
      printf("No more physical memory available!\n");
      return kExitFailure;
    default:
      printf("Allocated heap page at %p.\n", heap_start);
  }

  // Temporary heap. This is the same strategy we use in userboot stage 1, but
  // we should really have libc establish the heap.
  uint8_t *heap_bottom = reinterpret_cast<uint8_t *>(heap_start);
  uint8_t *heap_top = heap_bottom + kPageSize4M;
  size_t heap_size = kPageSize4M;

  if (heap_size < initrd_size * 2) {
    printf("WARN: The heap size may not be large enough to hold the vfs!\n");
  }

  user::InitializeUserHeap(heap_bottom, heap_top);
  printf("Initialized userboot stage 2 heap: %p - %p (%u bytes)\n", heap_bottom,
         heap_top, heap_size);

  printf("entry binary start: %p\n", &__binary_start);
  printf("entry binary end: %p\n", &__binary_end);
  size_t entry_binary_size = &__binary_end - &__binary_start;
  printf("entry binary size: %u\n", entry_binary_size);

  size_t vfs_size = initrd_size - entry_binary_size;
  std::unique_ptr<vfs::Directory> vfs =
      vfs::ParseUSTAR(initrd_data + entry_binary_size, vfs_size);

  printf("vfs:\n");
  vfs->Dump();

  const vfs::Directory *initrd_dir = vfs->getDir("initrd_files");
  if (const vfs::File *file = initrd_dir->getFile("userboot-stage2")) {
    std::unique_ptr<uint8_t> alloc(new uint8_t[sizeof(vfs_size) + vfs_size]);
    memcpy(alloc.get(), &vfs_size, sizeof(vfs_size));
    memcpy(alloc.get() + sizeof(vfs_size), initrd_data + entry_binary_size,
           vfs_size);
    LoadElfProgram(file->getContents().data(), alloc.get());
  } else {
    printf(
        "ERROR: Missing \"userboot-stage2\" binary. Exiting Userboot Stage 1 "
        "now.\n");
  }

  return 0;
}
