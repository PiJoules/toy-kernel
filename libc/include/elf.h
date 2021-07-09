#ifndef ELF_H_
#define ELF_H_

#include <_syscalls.h>
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

typedef struct elf32_shdr {
  Elf32_Word sh_name;
  Elf32_Word sh_type;
  Elf32_Word sh_flags;
  Elf32_Addr sh_addr;
  Elf32_Off sh_offset;
  Elf32_Word sh_size;
  Elf32_Word sh_link;
  Elf32_Word sh_info;
  Elf32_Word sh_addralign;
  Elf32_Word sh_entsize;
} Elf32_Shdr;

typedef struct {
  Elf32_Sword d_tag;  // Dynamic array tag controls the interpretation of d_un.
  union {
    Elf32_Word d_val;  // These Elf32_Word objects represent integer values
                       // with various interpretations.

    // These Elf32_Addr objects represent program virtual addresses. As
    // mentioned previously, a file’s virtual addresses might not match the
    // memory virtual addresses during execution. When interpreting addresses
    // contained in the dynamic structure, the dynamic linker computes actual
    // addresses, based on the original file value and the memory base address.
    // For consistency, files do not contain relocation entries to ‘‘correct’’
    // addresses in the dynamic structure.
    Elf32_Addr d_ptr;
  } d_un;
} Elf32_Dyn;

typedef struct {
  // This member gives the location at which to apply the relocation action. For
  // a relocatable file, the value is the byte offset from the beginning of the
  // section to the storage unit affected by the relocation. For an executable
  // file or a shared object, the value is the virtual address of the storage
  // unit affected by the relocation.
  Elf32_Addr r_offset;

  // This member gives both the symbol table index with respect to which the
  // relocation must be made, and the type of relocation to apply. For example,
  // a call instruction’s relocation entry would hold the symbol table index of
  // the function being called. If the index is STN_UNDEF, the undefined symbol
  // index, the relocation uses 0 as the ‘‘symbol value.’’ Relocation types are
  // processor-specific. When the text refers to a relocation entry’s relocation
  // type or symbol table index, it means the result of applying ELF32_R_TYPE or
  // ELF32_R_SYM, respectively, to the entry’s r_info member.
  Elf32_Word r_info;
} Elf32_Rel;

typedef struct {
  Elf32_Addr r_offset;
  Elf32_Word r_info;

  // This member specifies a constant addend used to compute the value to be
  // stored into the relocatable field.
  Elf32_Sword r_addend;
} Elf32_Rela;

#define ELF32_R_SYM(i) ((i) >> 8)
#define ELF32_R_TYPE(i) ((unsigned char)(i))
#define ELF32_R_INFO(s, t) (((s) << 8) + (unsigned char)(t))

typedef struct {
  // An index into the object file's symbol string table, which holds the
  // character representations of the symbol names. If the value is nonzero, it
  // represents a string table index that gives the symbol name. Otherwise, the
  // symbol table entry has no name.
  Elf32_Word st_name;
  Elf32_Addr st_value;
  Elf32_Word st_size;
  unsigned char st_info;
  unsigned char st_other;
  Elf32_Half st_shndx;
} Elf32_Sym;

// A: The addend used to compute the value of the relocatable field.
//
// B: The base address at which a shared object is loaded into memory during
// execution. Generally, a shared object file is built with a 0 base virtual
// address, but the execution address is different. See Program Header.
//
// G: The offset into the global offset table at which the address of the
// relocation entry's symbol resides during execution. See Global Offset Table
// (Processor-Specific).
//
// GOT: The address of the global offset table. See Global Offset Table
// (Processor-Specific).
//
// L: The section offset or address of the procedure linkage table entry for a
// symbol. See Procedure Linkage Table (Processor-Specific).
//
// P: The section offset or address of the storage unit being relocated,
// computed using r_offset.
//
// S: The value of the symbol whose index resides in the relocation entry.

#define R_386_RELATIVE 8  // word32; B + A

/* Legal values for d_tag (dynamic entry type).  */
#define DT_NULL 0              /* Marks end of dynamic section */
#define DT_NEEDED 1            /* Name of needed library */
#define DT_PLTRELSZ 2          /* Size in bytes of PLT relocs */
#define DT_PLTGOT 3            /* Processor defined value */
#define DT_HASH 4              /* Address of symbol hash table */
#define DT_STRTAB 5            /* Address of string table */
#define DT_SYMTAB 6            /* Address of symbol table */
#define DT_RELA 7              /* Address of Rela relocs */
#define DT_RELASZ 8            /* Total size of Rela relocs */
#define DT_RELAENT 9           /* Size of one Rela reloc */
#define DT_STRSZ 10            /* Size of string table */
#define DT_SYMENT 11           /* Size of one symbol table entry */
#define DT_INIT 12             /* Address of init function */
#define DT_FINI 13             /* Address of termination function */
#define DT_SONAME 14           /* Name of shared object */
#define DT_RPATH 15            /* Library search path (deprecated) */
#define DT_SYMBOLIC 16         /* Start symbol search here */
#define DT_REL 17              /* Address of Rel relocs */
#define DT_RELSZ 18            /* Total size of Rel relocs */
#define DT_RELENT 19           /* Size of one Rel reloc */
#define DT_PLTREL 20           /* Type of reloc in PLT */
#define DT_DEBUG 21            /* For debugging; unspecified */
#define DT_TEXTREL 22          /* Reloc might modify .text */
#define DT_JMPREL 23           /* Address of PLT relocs */
#define DT_BIND_NOW 24         /* Process relocations of object */
#define DT_INIT_ARRAY 25       /* Array with addresses of init fct */
#define DT_FINI_ARRAY 26       /* Array with addresses of fini fct */
#define DT_INIT_ARRAYSZ 27     /* Size in bytes of DT_INIT_ARRAY */
#define DT_FINI_ARRAYSZ 28     /* Size in bytes of DT_FINI_ARRAY */
#define DT_RUNPATH 29          /* Library search path */
#define DT_FLAGS 30            /* Flags for the object being loaded */
#define DT_ENCODING 32         /* Start of encoded range */
#define DT_PREINIT_ARRAY 32    /* Array with addresses of preinit fct*/
#define DT_PREINIT_ARRAYSZ 33  /* size in bytes of DT_PREINIT_ARRAY */
#define DT_NUM 34              /* Number used */
#define DT_LOOS 0x6000000d     /* Start of OS-specific */
#define DT_HIOS 0x6ffff000     /* End of OS-specific */
#define DT_LOPROC 0x70000000   /* Start of processor-specific */
#define DT_HIPROC 0x7fffffff   /* End of processor-specific */
#define DT_PROCNUM DT_MIPS_NUM /* Most used by any processor */

// This contains environment information shared between all of userboot. The
// purpose of this is to emulate a *nix environment. The filesystem for example
// will want to be exposed here since that is shared between processes.
struct GlobalEnvInfo {
  const void *raw_vfs_data;
  Handle raw_vfs_data_owner;
};

// Arginfo contains information on a per-process basis. Stuff like argument info
// or the directory this process is launched from goes here.
struct ArgInfo {
  GlobalEnvInfo env_info;

  // This buffer is packed/unpacked according to PackArgv and UnpackArgv.
  const char *packed_argv;
  size_t packed_argv_size;
  const char *pwd;
};

constexpr uint32_t kPageSize4M = 0x00400000;

extern "C" {

const GlobalEnvInfo *GetGlobalEnvInfo();
Handle GetRawVFSDataOwner();
const void *GetRawVFSData();

}  // extern "C"

void LoadElfProgram(const uint8_t *elf_data, const GlobalEnvInfo *env_info,
                    size_t argc = 0, const char *argv[ARG_MAX] = nullptr,
                    const char *pwd = nullptr);

// NOTE: This should always have the same value as USER_START in the kernel's
// paging.h.
#define USER_START UINT32_C(0x40000000)  // 1GB

#endif
