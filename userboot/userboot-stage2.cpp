#include <_syscalls.h>
#include <umalloc.h>
#include <userboot.h>
#include <vfs.h>

#include <cstdio>
#include <cstring>
#include <tuple>

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

sys::Handle RunFlatUserBinary(const vfs::Directory &vfs,
                              const std::string &filename,
                              void *arg = nullptr) {
  const vfs::File *file = vfs.getFile(filename);
  assert(file && "Could not find binary");

  size_t size = file->getContents().size();
  printf("%s is %u bytes\n", filename.c_str(), size);

  // Actually create and run the user tasks.
  sys::Handle handle = sys::CreateTask(file->getContents().data(), size, arg);
  printf("Created thread handle %u\n", handle);
  return handle;
}

constexpr const size_t kInitHeapSize = kPageSize4M;
constexpr const int kExitFailure = -1;

}  // namespace

int main(int argc, char **argv) {
  // FIXME: This should be handled from libc setup.
  __use_debug_log = true;

  printf("\n=== USERBOOT STAGE 2 ===\n\n");
  printf(
      "  This contains the rest of the userboot code. Userboot just simply \n"
      "  launches a user shell that can be used for running various \n"
      "  programs.\n\n");

  // Allocate space for the heap on the page immediately after where this is
  // mapped.
  // TODO: Formalize this more. Setup of the heap and malloc should be the duty
  // of libc.
  void *heap_start = NextPage();
  printf("heap_start: %p\n", heap_start);
  auto val = sys_map_page(heap_start);
  switch (val) {
    case MAP_UNALIGNED_ADDR:
      printf(
          "Attempting to map virtual address %p which is not aligned to "
          "page.\n",
          heap_start);
      return kExitFailure;
    case MAP_ALREADY_MAPPED:
      printf("Attempting to map virtual address %p which is already mapped.\n",
             heap_start);
      return kExitFailure;
    case MAP_OOM:
      printf("No more physical memory available!\n");
      return kExitFailure;
    default:
      printf("Allocated heap page at %p.\n", heap_start);
  }

  // Temporary heap. This is the same strategy we use in userboot stage 1, but
  // we should really have libc establish the heap.
  uint8_t *heap_bottom = reinterpret_cast<uint8_t *>(heap_start);
  uint8_t *heap_top = heap_bottom + kInitHeapSize;

  // FIXME: This should be handled from libc setup.
  user::InitializeUserHeap(heap_bottom, heap_top);
  printf("Initialized userboot stage 2 heap: %p - %p (%u bytes)\n", heap_bottom,
         heap_top, kInitHeapSize);

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

  if (kInitHeapSize < initrd_size * 2) {
    printf("WARN: The heap size may not be large enough to hold the vfs!\n");
  }

  uint8_t *vfs_data_loc = (uint8_t *)vfs_loc + sizeof(initrd_size);
  printf("vfs start: %p\n", vfs_data_loc);

  std::unique_ptr<uint8_t> vfs_data_holder(new uint8_t[initrd_size]);
  uint8_t *initrd_data = vfs_data_holder.get();
  sys_copy_from_task(parent, initrd_data, vfs_data_loc, initrd_size);

  std::unique_ptr<vfs::Directory> vfs =
      vfs::ParseUSTAR(initrd_data, initrd_size);
  printf("vfs:\n");
  vfs->Dump();

  // Check starting a user task via syscall.
  printf("Trying test_user_program.bin ...\n");
  const vfs::Directory *initrd_dir = vfs->getDir("initrd_files");
  auto handle1 = RunFlatUserBinary(*initrd_dir, "test_user_program.bin");
  auto handle2 = RunFlatUserBinary(*initrd_dir, "test_user_program.bin");
  sys::DestroyTask(handle1);
  sys::DestroyTask(handle2);
  printf("Finished test_user_program.bin.\n");

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

  printf("\nWelcome! Type \"help\" for commands\n");

  char buffer[kCmdBufferSize];
  while (1) {
    printf("shell> ");
    DebugRead(buffer);

    if (ShouldShutdown(buffer)) break;
  }

  return 0;
}
