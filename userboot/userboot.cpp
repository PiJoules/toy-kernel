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
      return -1;
    case MAP_ALREADY_MAPPED:
      printf("Attempting to map virtual address %p which is already mapped.\n",
             heap_start);
      return -1;
    case MAP_OOM:
      printf("No more physical memory available!\n");
      return -1;
    default:
      printf("Allocated heap page at %p.\n", heap_start);
  }

  // Temporary heap. This is the same strategy we use in userboot stage 1, but
  // we should really have libc establish the heap.
  uint8_t *heap_bottom = reinterpret_cast<uint8_t *>(heap_start);
  uint8_t *heap_top = heap_bottom + kPageSize4M;
  size_t heap_size = kPageSize4M;

  if (heap_size < vfs_size * 2) {
    printf("WARN: The heap size may not be large enough to hold the vfs!\n");
  }

  // FIXME: This is here because not all the memory in the heap is zero-mapped,
  // which causes the vfs to not work correctly. This is a bug in the vfs and
  // should be fixed.
  memset(heap_bottom, 0, heap_size);

  user::InitializeUserHeap(heap_bottom, heap_top);
  printf("Initialized userboot stage 2 heap: %p - %p (%u bytes)\n", heap_bottom,
         heap_top, heap_size);

  std::unique_ptr<vfs::Directory> vfs;
  uint32_t entry_offset;
  vfs = vfs::ParseVFS(vfs_data, vfs_data + vfs_size, entry_offset);

  printf("entry point offset: %u\n", entry_offset);
  printf("vfs:\n");
  vfs->Dump();

  // Check starting a user task via syscall.
  printf("Trying test_user_program.bin ...\n");
  RunFlatUserBinary(*vfs, "test_user_program.bin");
  printf("Finished test_user_program.bin.\n");

  if (const vfs::Node *file = vfs->getFile("userboot-stage2")) {
    LoadElfProgram(file->AsFile().contents.data(), arg);
  } else {
    printf(
        "ERROR: Missing \"userboot-stage2\" binary. Exiting Userboot Stage 1 "
        "now.\n");
  }

  return 0;
}
