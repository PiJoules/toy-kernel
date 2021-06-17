#include <_log.h>
#include <_syscalls.h>
#include <assert.h>
#include <elf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <memory>
#include <new>

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

  DEBUG("elf data loc: %p\n", elf_data);

  // NOTE: A binary can still be marked as a shared object file (ET_DYN) yet
  // still be an executable. See the answers in
  // https://stackoverflow.com/q/34519521/2775471.
  DEBUG("program type: %d\n", hdr->e_type);

  uint32_t program_entry_point = hdr->e_entry;
  DEBUG("program entry point: %x\n", program_entry_point);

  // Just dump the loadable segments.
  const auto *phdr =
      reinterpret_cast<const Elf32_Phdr *>(elf_data + hdr->e_phoff);
  for (int i = 0; i < hdr->e_phnum; ++i) {
    const auto &segment = phdr[i];
    if (segment.p_type != PT_LOAD) continue;
    DEBUG("LOAD segment Offset: %x, VirtAddr: %p, memsz: %x\n",
          segment.p_offset, (void *)segment.p_vaddr, segment.p_memsz);
  }

  // Note that the ELF standard guarantees that all loadable segments are sorted
  // by p_vaddr.
  const Elf32_Phdr *first_loadable_segment = nullptr;
  for (int i = 0; i < hdr->e_phnum &&
                  (first_loadable_segment = &phdr[i])->p_type != PT_LOAD;
       ++i) {}
  assert(first_loadable_segment && "Could not find any loadable segments.");

  assert(hdr->e_phnum > 0 && "No phdrs.");
  const Elf32_Phdr *last_loadable_segment = &phdr[hdr->e_phnum - 1];
  while (last_loadable_segment->p_type != PT_LOAD) --last_loadable_segment;
  assert(last_loadable_segment->p_vaddr >= first_loadable_segment->p_vaddr &&
         "Out of order loadable segments.");
  assert(
      first_loadable_segment->p_vaddr <= program_entry_point &&
      program_entry_point <
          last_loadable_segment->p_vaddr +
              static_cast<size_t>(last_loadable_segment->p_memsz) &&
      "The program entry point is not within the range of loadable segments.");

  // This is the offset between the virtual address of the first loadable
  // segment and the virtual address of the end of the last loadable segment.
  size_t loadable_segment_span =
      last_loadable_segment->p_vaddr +
      static_cast<size_t>(last_loadable_segment->p_memsz) -
      first_loadable_segment->p_vaddr;

  // Get the offset into the elf file that this data is located at given a
  // virtual address.
  //
  // `p_type` of -1 indicates any p_type.
  auto get_offset_bias = [phdr, hdr](uint32_t vaddr, int p_type = -1) {
    for (int i = 0; i < hdr->e_phnum; ++i) {
      const auto &segment = phdr[i];

      // Check for the right type if applicable.
      if (p_type >= 0 && segment.p_type != p_type) continue;

      // Check if the virtual address is within the bounds of this segment.
      if (segment.p_vaddr > vaddr ||
          vaddr >= segment.p_vaddr + static_cast<size_t>(segment.p_memsz))
        continue;

      // Finally get the offset into the file.
      return segment.p_offset + (vaddr - segment.p_vaddr);
    }

    __builtin_unreachable();
    return UINT32_C(0);
  };

  // FIXME: Rather than actually mmap'ing the individual segments (which we
  // should eventually do), we just allocate one big chunk that we transpose the
  // individual segments on, where the start of the chunk represents the virtual
  // address of the first loadable segment.
  std::unique_ptr<uint8_t> program(new uint8_t[loadable_segment_span]);
  memset(program.get(), 0, loadable_segment_span);
  for (int i = 0; i < hdr->e_phnum; ++i) {
    const auto &segment = phdr[i];
    if (segment.p_type != PT_LOAD) continue;

    size_t size = static_cast<size_t>(segment.p_memsz);
    uint32_t file_offset = segment.p_offset;
    uint32_t vaddr = segment.p_vaddr;
    memcpy(program.get() + vaddr - first_loadable_segment->p_vaddr,
           elf_data + file_offset, size);
  }

  // Dynamic relocations.
  const Elf32_Dyn *dynamic = nullptr;
  for (int i = 0; i < hdr->e_phnum; ++i) {
    if (phdr[i].p_type == PT_DYNAMIC) {
      dynamic =
          reinterpret_cast<const Elf32_Dyn *>(phdr[i].p_offset + elf_data);
      break;
    }
  }
  if (dynamic) {
    // Get the following entries in the .dynamic section for a given field.
    auto get_dynamic_entry = [](const Elf32_Dyn *dynamic, int field) {
      for (; dynamic->d_tag != DT_NULL; dynamic++) {
        if (dynamic->d_tag == field) return dynamic->d_un.d_val;
      }
      return 0;
    };
    const Elf32_Rel *relocs = (Elf32_Rel *)get_dynamic_entry(dynamic, DT_REL);
    assert(relocs && "Could not find .rel.dyn section");
    size_t relocs_size =
        static_cast<size_t>(get_dynamic_entry(dynamic, DT_RELSZ));
    assert(relocs_size && "Found an empty relocation section.");

    uint32_t offset_bias = get_offset_bias(reinterpret_cast<uint32_t>(relocs));
    relocs = reinterpret_cast<const Elf32_Rel *>(elf_data + offset_bias);
    for (int i = 0; i < relocs_size / sizeof(Elf32_Rel); ++i) {
      const Elf32_Rel *reloc = &relocs[i];
      int reloc_sym = ELF32_R_SYM(reloc->r_info);
      int reloc_type = ELF32_R_TYPE(reloc->r_info);

      switch (reloc_type) {
        case R_386_RELATIVE: {
          // Created by the link-editor for dynamic objects. Its offset member
          // gives the location within a shared object that contains a value
          // representing a relative address. The runtime linker computes the
          // corresponding virtual address by adding the virtual address at
          // which the shared object is loaded to the relative address.
          // Relocation entries for this type must specify 0 for the symbol
          // table index.
          uint32_t *loc =
              reinterpret_cast<uint32_t *>(program.get() + reloc->r_offset);
          *loc += USER_START;
          break;
        }
        default:
          WARN("Unknown reloc %p: sym %d, type %d\n", reloc, reloc_sym,
               reloc_type);
      }
    }
  }

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

  sys::Handle handle = sys::CreateTask(
      program.get(), loadable_segment_span,
      /*arg=*/&arginfo,
      /*entry_offset=*/program_entry_point - first_loadable_segment->p_vaddr);
  sys::DestroyTask(handle);
}
