#include <_syscalls.h>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <umalloc.h>
#include <vfs.h>

#include <new>
#include <tuple>

extern bool __use_debug_log;

namespace {

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

bool IsValidElf(const Elf32_Ehdr *hdr) {
  return (hdr->e_ident[0] == ELFMAG0 && hdr->e_ident[1] == ELFMAG1 &&
          hdr->e_ident[2] == ELFMAG2 && hdr->e_ident[3] == ELFMAG3);
}

// NOTE: This should always have the same value as USER_START in the kernel's
// paging.h.
#define USER_START UINT32_C(0x40000000)  // 1GB

void LoadElfProgram(const uint8_t *elf_data) {
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

    printf("v_begin: %x\n", v_begin);
    printf("offset to start of elf: %x\n", p_entry->p_offset);
    printf("size: %u\n", p_entry->p_filesz);

    const uint8_t *start = elf_data + p_entry->p_offset;
    printf("eip[0]: %x\n", start[0]);

    sys::Handle handle =
        sys::CreateTask(elf_data + p_entry->p_offset, p_entry->p_filesz);
    printf("Created handle %u for elf file\n", handle);
    sys::DestroyTask(handle);
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

constexpr char CR = 13;  // Carriage return

char GetChar() {
  char c;
  while (!sys::DebugRead(c)) {}
  return c;
}

// Store typed characters into a buffer while also saving the command until the
// next ENTER.
void DebugRead(char *buffer) {
  while (1) {
    char c = GetChar();
    if (c == CR) {
      *buffer = 0;
      put('\n');
      return;
    }

    *(buffer++) = c;
    put(c);
  }
}

constexpr size_t kCmdBufferSize = 1024;

bool TriggerInvalidOpcodeException() {
  asm volatile("ud2\n");
  return false;
}

using CmdInfo = std::tuple<const char *, const char *, bool (*)()>;

bool DumpCommands();
bool Shutdown() { return true; }

constexpr CmdInfo kCmds[] = {
    {"help", "Dump commands.", DumpCommands},
    {"shutdown", "Exit userboot.", Shutdown},
    {"invalid-opcode", "Trigger an invalid opcode exception",
     TriggerInvalidOpcodeException},
};
constexpr size_t kNumCmds = sizeof(kCmds) / sizeof(kCmds[0]);

bool DumpCommands() {
  for (size_t i = 0; i < kNumCmds; ++i)
    printf("%s - %s\n", std::get<0>(kCmds[i]), std::get<1>(kCmds[i]));
  return false;
}

}  // namespace

extern "C" uint8_t heap_bottom, heap_top;

extern "C" int __user_main(void *arg) {
  __use_debug_log = true;

  // This is the dummy argument we expect from the kernel.
  printf("arg: %p\n", arg);
  size_t vfs_size;
  memcpy(&vfs_size, arg, sizeof(vfs_size));
  printf("vfs size: %u\n", vfs_size);
  uint8_t *vfs_data = (uint8_t *)arg + sizeof(vfs_size);
  printf("vfs start: %p\n", vfs_data);

  printf("heap_bottom: %p\n", &heap_bottom);
  printf("heap_top: %p\n", &heap_top);

  size_t heap_space = &heap_top - &heap_bottom;
  printf("heap space: %u\n", heap_space);
  assert(heap_space > vfs_size &&
         "The heap allocation is not large enough to hold the vfs. More space "
         "should be allocated for bootstrapping.");

  user::InitializeUserHeap();
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

  printf("\nWelcome! Type \"help\" for commands\n");

  if (const vfs::Node *file = vfs->getFile("test-elf")) {
    LoadElfProgram(file->AsFile().contents.data());
  }

  auto ShouldShutdown = [](char *buffer) -> bool {
    for (size_t i = 0; i < kNumCmds; ++i) {
      const char *name = std::get<0>(kCmds[i]);
      if (strcmp(buffer, name) == 0) {
        // Only `shutdown` returns true.
        return std::get<2>(kCmds[i])();
      }
    }

    printf("Unknown command: %s\n", buffer);
    return false;
  };

  char buffer[kCmdBufferSize];
  while (1) {
    printf("shell> ");
    DebugRead(buffer);

    if (ShouldShutdown(buffer)) break;
  }

  return 0;
}
