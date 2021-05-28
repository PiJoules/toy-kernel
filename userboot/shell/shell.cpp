#include <_syscalls.h>
#include <elf.h>
#include <vfs.h>

#include <cassert>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <memory>

namespace {

// FIXME: We would at some point like to accept commands longer than 1024
// characters.
constexpr const size_t kCmdBufferSize = 1024;
constexpr const char CR = 13;  // Carriage return

// Store typed characters into a buffer while also saving the command until the
// next ENTER.
void DebugRead(char *buffer) {
  while (1) {
    int c = getchar();
    assert(c != EOF);
    if (c == CR) {
      *buffer = 0;
      putchar('\n');
      return;
    }

    *(buffer++) = static_cast<char>(c);
    putchar(c);
  }
}

// Split up the string into argv. This function populates argv by pointing to
// strings into a argv_buffer, which will contain  a series of null terminated
// strings copied from argstr. The ending of argv will be indicated with a null
// pointer.
void ArgvFromArgString(const char *argstr, size_t argv_buffer_len,
                       char *argv[ARG_MAX], char argv_buffer[argv_buffer_len]) {
  char c;
  bool parsing_arg = false;
  while ((c = *(argstr++)) != 0) {
    if (isspace(c)) {
      if (parsing_arg) {
        // We found whitespace after parsing an argument. Terminate this
        // argument.
        *(argv_buffer++) = 0;
      }
      parsing_arg = false;
    } else {
      if (!parsing_arg) {
        // This is the first character in an argument we're parsing.
        assert(*argv_buffer && "Should not be pointing to a null terminator.");
        *(argv++) = argv_buffer;
      }
      *(argv_buffer++) = c;
      parsing_arg = true;
    }
  }
  *argv_buffer = 0;
  *argv = nullptr;
}

}  // namespace

extern const void *__raw_vfs_data;
extern "C" Handle GetRawVFSDataOwner();

int main([[maybe_unused]] int argc, [[maybe_unused]] char **argv) {
  const void *vfs_loc = __raw_vfs_data;
  Handle vfs_owner = GetRawVFSDataOwner();

  // The first argument will be a pointer to vfs data in the owner task's
  // address space. Map some virtual address in this task's address space to
  // the page with the vfs so we can read from it and construct a vfs object.
  auto vfs_loc_int = reinterpret_cast<uintptr_t>(vfs_loc);
  uint32_t loc_offset = vfs_loc_int % kPageSize4M;
  auto vfs_loc_page = vfs_loc_int - loc_offset;
  uint8_t *this_vfs_page;
  sys_share_page(vfs_owner, reinterpret_cast<void **>(&this_vfs_page),
                 reinterpret_cast<void *>(vfs_loc_page));
  uint8_t *this_vfs = this_vfs_page + loc_offset;

  // First get the size and do a santity check that the remainder of the vfs
  // data is still on the page we mapped. Otherwise, we'll need to map another
  // page.
  size_t initrd_size;
  memcpy(&initrd_size, this_vfs, sizeof(initrd_size));
  printf("initrd size: %u\n", initrd_size);
  assert(loc_offset + initrd_size <= kPageSize4M &&
         "FIXME: Will need to allocate more pages. Initrd does not fit in one "
         "page.");

  // Everything is mapped up. We can safely use this vfs pointer.
  uint8_t *initrd_data = this_vfs + sizeof(initrd_size);

  std::unique_ptr<vfs::Directory> vfs =
      vfs::ParseUSTAR(initrd_data, initrd_size);
  printf("vfs:\n");
  vfs->Dump();

  char buffer[kCmdBufferSize];
  while (1) {
    printf("> ");
    DebugRead(buffer);

    size_t bufferlen = strlen(buffer) + 1;
    char *argv[ARG_MAX];
    char argv_buffer[bufferlen];
    ArgvFromArgString(buffer, bufferlen, argv, argv_buffer);
    size_t argc = 0;
    while (argv[argc]) ++argc;
    if (!argc) continue;

    // If it happens to be an executable file, execute it.
    // Note that we can pass `cmd` here because the std::string ctor will parse
    // it as a null-terminated string up to the first argument.
    if (const vfs::File *file = vfs->getFile(argv[0])) {
      LoadElfProgram(file->getContents().data(), __raw_vfs_data,
                     GetRawVFSDataOwner(), argc,
                     const_cast<const char **>(argv));
    } else {
      printf("Unknown command '%s'\n", argv[0]);
    }
  }

  // This isn't needed since this task's address space is reclaimed once it's
  // finished, but it's good to know how to use it.
  sys_unmap_page(initrd_data);

  return 0;
}
