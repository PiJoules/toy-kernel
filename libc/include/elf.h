#ifndef ELF_H_
#define ELF_H_

#include <limits.h>
#include <stdint.h>

// Elf types and data structures are taken from
// http://www.skyfree.org/linux/references/ELF_Format.pdf.
typedef uint32_t Elf32_Addr;  // Unsigned program address
typedef uint16_t Elf32_Half;  // Unsigned medium integer
typedef uint32_t Elf32_Off;   // Unsigned file offset
typedef int32_t Elf32_Sword;  // Signed large integer
typedef int32_t Elf32_Word;   // Unsigned large integer
static_assert(sizeof(Elf32_Addr) == 4 && alignof(Elf32_Addr) == 4);
static_assert(sizeof(Elf32_Half) == 2 && alignof(Elf32_Half) == 2);
static_assert(sizeof(Elf32_Off) == 4 && alignof(Elf32_Off) == 4);
static_assert(sizeof(Elf32_Sword) == 4 && alignof(Elf32_Sword) == 4);
static_assert(sizeof(Elf32_Word) == 4 && alignof(Elf32_Word) == 4);

#define EI_NIDENT 16

// ELF header
typedef struct {
  unsigned char e_ident[16]; /* ELF identification */
  Elf32_Half e_type;         /* 2 (exec file) */
  Elf32_Half e_machine;      /* 3 (intel architecture) */
  Elf32_Word e_version;      /* 1 */
  Elf32_Addr e_entry;        /* starting point */
  Elf32_Off e_phoff;         /* program header table offset */
  Elf32_Off e_shoff;         /* section header table offset */
  Elf32_Word e_flags;        /* various flags */
  Elf32_Half e_ehsize;       /* ELF header (this) size */
  Elf32_Half e_phentsize;    /* program header table entry size */
  Elf32_Half e_phnum;        /* number of entries */
  Elf32_Half e_shentsize;    /* section header table entry size */
  Elf32_Half e_shnum;        /* number of entries */
  Elf32_Half e_shstrndx;     /* index of the section name string table */
} Elf32_Ehdr;

// Program header
typedef struct {
  Elf32_Word p_type; /* type of segment */
  Elf32_Off p_offset;
  Elf32_Addr p_vaddr;
  Elf32_Addr p_paddr;
  Elf32_Word p_filesz;
  Elf32_Word p_memsz;
  Elf32_Word p_flags;
  Elf32_Word p_align;
} Elf32_Phdr;

// p_type
#define PT_NULL 0
#define PT_LOAD 1
#define PT_DYNAMIC 2
#define PT_INTERP 3
#define PT_NOTE 4
#define PT_SHLIB 5
#define PT_PHDR 6
#define PT_LOPROC 0x70000000
#define PT_HIPROC 0x7fffffff

#define ELFMAG0 0x7F
#define ELFMAG1 'E'
#define ELFMAG2 'L'
#define ELFMAG3 'F'

inline bool IsValidElf(const Elf32_Ehdr *hdr) {
  return (hdr->e_ident[0] == ELFMAG0 && hdr->e_ident[1] == ELFMAG1 &&
          hdr->e_ident[2] == ELFMAG2 && hdr->e_ident[3] == ELFMAG3);
}

// Segment flags.
// https://docs.oracle.com/cd/E19683-01/816-1386/6m7qcoblk/index.html#chapter6-tbl-39
#define PF_X 1       // Executable
#define PF_W 2       // Writable
#define PF_R 4       // Readable
#define PF_MASKPROC  // Unspecified

struct ArgInfo {
  const void *raw_vfs_data;
  Handle raw_vfs_data_owner;

  // This buffer is packed/unpacked according to PackArgv and UnpackArgv.
  const char *packed_argv;
  size_t packed_argv_size;
};

constexpr uint32_t kPageSize4M = 0x00400000;

void LoadElfProgram(const uint8_t *elf_data, const void *raw_vfs_data,
                    Handle raw_vfs_data_owner, size_t argc = 0,
                    const char *argv[ARG_MAX] = nullptr);

#endif
