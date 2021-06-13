#include <_syscalls.h>
#include <assert.h>
#include <elf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>

// NOTE: This should always have the same value as USER_START in the kernel's
// paging.h.
#define USER_START UINT32_C(0x40000000)  // 1GB

namespace {

// packed_size must be large enough to hold all the strings in argv.
void PackArgv(size_t argc, const char **argv, size_t packed_size,
              char packed_argv[packed_size]) {
  for (size_t i = 0; i < argc; ++i) {
    const char *arg = argv[i];
    size_t len = strlen(arg);
    memcpy(packed_argv, arg, len);
    packed_argv[len] = 0;
    packed_argv += len + 1;  // +1 for the null terminator.
  }
}

}  // namespace

void LoadElfProgram(const uint8_t *elf_data, const GlobalEnvInfo *env_info,
                    size_t argc, const char **argv, const char *pwd) {
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

  ArgInfo arginfo = {
      .env_info = *env_info,
  };
  if (argc) {
    size_t packed_argv_size = 0;
    for (size_t i = 0; i < argc; ++i) {
      packed_argv_size += strlen(argv[i]) + 1;
    }
    arginfo.packed_argv_size = packed_argv_size;
  } else {
    arginfo.packed_argv_size = 0;
  }

  // Ensure that we have at least a non-zero vla.
  char packed_argv[arginfo.packed_argv_size ? arginfo.packed_argv_size : 1];

  if (argc) {
    PackArgv(argc, argv, arginfo.packed_argv_size, packed_argv);
    arginfo.packed_argv = packed_argv;
  } else {
    arginfo.packed_argv = nullptr;
  }

  arginfo.pwd = pwd;

  sys::Handle handle = sys::CreateTask(elf_data + start_offset, cpysize,
                                       /*arg=*/&arginfo,
                                       /*entry_offset=*/entry_offset);
  sys::DestroyTask(handle);
}
