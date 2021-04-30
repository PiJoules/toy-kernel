#include <_syscalls.h>
#include <vfs.h>

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <memory>

namespace {

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
  void *vfs_loc = argv[0];

  Handle parent = sys_get_parent_task();
  printf("parent: %u\n", parent);
  uint32_t parent_id = sys_get_parent_task_id();
  printf("parent id: %u\n", parent_id);

  size_t initrd_size;
  sys_copy_from_task(parent, &initrd_size, vfs_loc, sizeof(initrd_size));
  printf("initrd size: %u\n", initrd_size);

  uint8_t *vfs_data_loc = (uint8_t *)vfs_loc + sizeof(initrd_size);
  printf("vfs start: %p\n", vfs_data_loc);

  std::unique_ptr<uint8_t> vfs_data_holder(new uint8_t[initrd_size]);
  uint8_t *initrd_data = vfs_data_holder.get();
  sys_copy_from_task(parent, initrd_data, vfs_data_loc, initrd_size);

  std::unique_ptr<vfs::Directory> vfs =
      vfs::ParseUSTAR(initrd_data, initrd_size);
  printf("vfs:\n");
  vfs->Dump();

  char buffer[kCmdBufferSize];
  while (1) {
    printf("> ");
    DebugRead(buffer);
  }
  return 0;
}
