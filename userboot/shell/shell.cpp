#include <_syscalls.h>
#include <elf.h>
#include <vfs.h>

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <memory>

namespace {

constexpr uint32_t kPageSize4M = 0x00400000;

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

}  // namespace

int main(int argc, char **argv) {
  printf("argc: %d\n", argc);
  printf("argv: %p\n", argv);
  printf("argv[0]: %p\n", argv[0]);
  void *parent_vfs_loc = argv[0];

  Handle parent = sys_get_parent_task();
  printf("parent: %u\n", parent);

  // The first argument will be a pointer to vfs data in the parent task's
  // address space. Map some virtual address in this task's address space to
  // the page with the vfs so we can read from it and construct a vfs object.
  auto vfs_loc_int = reinterpret_cast<uintptr_t>(parent_vfs_loc);
  uint32_t loc_offset = vfs_loc_int % kPageSize4M;
  auto vfs_loc_page = vfs_loc_int - loc_offset;
  uint8_t *this_vfs_page;
  sys_share_page(parent, reinterpret_cast<void **>(&this_vfs_page),
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

    // If it happens to be an executable file, execute it.
    if (const vfs::File *file = vfs->getFile(buffer)) {
      printf("%s is a file\n", buffer);
      LoadElfProgram(file->getContents().data(), parent_vfs_loc);
    } else {
      printf("%s is not a file\n", buffer);
    }
  }

  // This isn't needed since this task's address space is reclaimed once it's
  // finished, but it's good to know how to use it.
  sys_unmap_page(initrd_data);

  return 0;
}
