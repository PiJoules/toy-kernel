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
