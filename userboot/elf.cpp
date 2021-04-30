#include <_syscalls.h>
#include <elf.h>

#include <algorithm>
#include <cassert>
#include <cstdio>

// NOTE: This should always have the same value as USER_START in the kernel's
// paging.h.
#define USER_START UINT32_C(0x40000000)  // 1GB

void LoadElfProgram(const uint8_t *elf_data, void *arg) {
  const auto *hdr = reinterpret_cast<const Elf32_Ehdr *>(elf_data);
  assert(IsValidElf(hdr) && "Invalid elf program");

  // NOTE: A binary can still be marked as a shared object file (ET_DYN) yet
  // still be an executable. See the answers in
  // https://stackoverflow.com/q/34519521/2775471.
  printf("program type: %d\n", hdr->e_type);

  uint32_t vaddr_entry = hdr->e_entry;
  printf("program entry: %p\n", (void *)vaddr_entry);

  // Represents the base virtual address that this DSO should be loaded to. For
  // non-PIEs, it should start at USER_START. For PIEs, it will probably start
  // at zero.
  int64_t vaddr_start = INT64_MAX;
  uint32_t vaddr_end = 0;

  const auto *p_entry =
      reinterpret_cast<const Elf32_Phdr *>(elf_data + hdr->e_phoff);
  int64_t start_offset = -1;
  size_t end_offset = 0;
  for (int i = 0; i < hdr->e_phnum; ++i, ++p_entry) {
    if (p_entry->p_type != PT_LOAD) continue;

    size_t size = static_cast<size_t>(p_entry->p_filesz);
    uint32_t v_begin = p_entry->p_vaddr;
    vaddr_start = std::min(vaddr_start, static_cast<int64_t>(v_begin));
    vaddr_end = std::max(vaddr_end, v_begin + size);

    size_t offset = p_entry->p_offset;
    assert(p_entry->p_filesz >= 0);
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

    if (start_offset < 0) { start_offset = p_entry->p_offset; }

    end_offset = p_entry->p_offset + size;
  }

  assert(vaddr_start >= 0);
  assert(vaddr_entry >= vaddr_start);
  uint32_t entry_offset = vaddr_entry - static_cast<uint32_t>(vaddr_start);

  // Use these as a reasonable buffer size to map points to.
  printf("base virtual address: %p\n", (void *)vaddr_start);
  printf("end virtual address: %p\n", (void *)vaddr_end);

  printf("entry offset: %x\n", entry_offset);
  printf("start offset: %x\n", static_cast<uint32_t>(start_offset));
  printf("end offset: %x\n", end_offset);
  assert(entry_offset < end_offset &&
         "Entry point exceeds end of the last LOADable address");

  assert(start_offset >= 0 && end_offset && end_offset > start_offset);
  size_t cpysize = end_offset - static_cast<size_t>(start_offset);
  sys::Handle handle = sys::CreateTask(elf_data + start_offset, cpysize,
                                       /*arg=*/arg,
                                       /*entry_offset=*/entry_offset);
  printf("Created handle %u for elf file\n", handle);
  sys::DestroyTask(handle);
}
