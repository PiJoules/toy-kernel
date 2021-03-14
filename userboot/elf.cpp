#include <_syscalls.h>
#include <elf.h>

#include <cassert>
#include <cstdio>

// NOTE: This should always have the same value as USER_START in the kernel's
// paging.h.
#define USER_START UINT32_C(0x40000000)  // 1GB

void LoadElfProgram(const uint8_t *elf_data, void *arg) {
  const auto *hdr = reinterpret_cast<const Elf32_Ehdr *>(elf_data);
  assert(IsValidElf(hdr) && "Invalid elf program");

  const auto *p_entry =
      reinterpret_cast<const Elf32_Phdr *>(elf_data + hdr->e_phoff);
  size_t start_offset = 0;
  size_t end_offset = 0;
  for (int i = 0; i < hdr->e_phnum; ++i, ++p_entry) {
    if (p_entry->p_type != PT_LOAD) continue;

    uint32_t v_begin = p_entry->p_vaddr;
    if (v_begin < USER_START) {
      printf("Warning: Skipped to load %d bytes to %x\n", p_entry->p_filesz,
             v_begin);
      continue;
    }

    size_t offset = p_entry->p_offset;
    assert(p_entry->p_filesz >= 0);
    size_t size = static_cast<size_t>(p_entry->p_filesz);
    printf("LOAD segment Offset: %x, VirtAddr: %p, filesz: %x\n", offset,
           (void *)v_begin, size);

    if (!size) {
      printf("Skipping segment of size 0\n");
      continue;
    }

    if (end_offset && end_offset > offset + size) {
      printf(
          "WARNING: The next LOAD segment does not come after the previous "
          "load (%x + %x < %x).\n",
          offset, size, end_offset);
      __builtin_trap();
    }

    if (!start_offset) {
      start_offset = p_entry->p_offset;
      assert(p_entry->p_flags & PF_X &&
             "Expected the first LOAD segment to be executable (since .text "
             "should be the first section in the binary)");
      assert(v_begin == USER_START &&
             "Expected the first LOAD segment to start at USER_START");
    }

    end_offset = p_entry->p_offset + size;
  }

  assert(start_offset && end_offset && end_offset > start_offset);
  size_t cpysize = end_offset - start_offset;
  sys::Handle handle = sys::CreateTask(elf_data + start_offset, cpysize, arg);
  printf("Created handle %u for elf file\n", handle);
  sys::DestroyTask(handle);
}
