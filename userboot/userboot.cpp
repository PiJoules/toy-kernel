#include <_syscalls.h>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <umalloc.h>
#include <vfs.h>

#include <new>

extern bool __use_debug_log;

namespace {

constexpr char CR = 13;  // Carriage return

char GetChar() {
  char c;
  while (!sys::DebugRead(c)) {}
  return c;
}

// Store typed characters into a buffer while also saving the command until the
// next ENTER.
// FIXME: Getting characters works by making a syscall to the kernel which waits
// for a new character from serial.
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

void DumpCommands() {
  printf(
      "help - Dump commands.\n"
      "shutdown - Exit userboot.\n");
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
  vfs = vfs::ParseVFS(vfs_data, vfs_data + vfs_size);

  printf("vfs:\n");
  vfs->Dump();

  printf("\nWelcome! Type \"help\" for commands\n");

  char buffer[kCmdBufferSize];
  while (1) {
    printf("shell> ");
    DebugRead(buffer);

    if (strcmp(buffer, "help") == 0) {
      DumpCommands();
    } else if (strcmp(buffer, "shutdown") == 0) {
      printf("Shutting down...\n");
      break;
    } else {
      printf("Unknown command: %s\n", buffer);
    }
  }

  return 0;
}
