#include <_syscalls.h>
#include <allocator.h>
#include <stdio.h>
#include <string.h>
#include <umalloc.h>
#include <userboot.h>
#include <vfs.h>

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

}  // namespace

namespace user {
extern size_t GetHeapUsed();
}  // namespace user

int main(int argc, char **argv) {
  // FIXME: This should be handled from libc setup.
  __use_debug_log = true;

  printf("\n=== USERBOOT STAGE 2 ===\n\n");
  printf(
      "  This contains the rest of the userboot code. Userboot just simply \n"
      "  launches a user shell that can be used for running various "
      "programs.\n\n");

  // Temporary heap. This is the same strategy we use in userboot stage 1, but
  // we should really have libc establish the heap.
  alignas(16) uint8_t kHeap[kHeapSize];
  memset(kHeap, 0, kHeapSize);
  uint8_t *kHeapBottom = kHeap;
  uint8_t *kHeapTop = &kHeap[kHeapSize];

  user::InitializeUserHeap(kHeapBottom, kHeapTop);
  printf("Initialized userboot stage 2 heap: %p - %p (%u bytes)\n", kHeapBottom,
         kHeapTop, kHeapSize);

  printf("argc: %d\n", argc);
  printf("argv: %p\n", argv);
  printf("argv[0]: %p\n", argv[0]);
  void *vfs_loc = argv[0];

  Handle parent = sys_get_parent_task();
  printf("parent: %u\n", parent);
  uint32_t parent_id = sys_get_parent_task_id();
  printf("parent id: %u\n", parent_id);

  size_t vfs_size;
  sys_copy_from_task(parent, &vfs_size, vfs_loc, sizeof(vfs_size));
  printf("vfs size: %u\n", vfs_size);
  uint8_t *vfs_data_loc = (uint8_t *)vfs_loc + sizeof(vfs_size);
  printf("vfs start: %p\n", vfs_data_loc);

  // uint8_t *vfs_data = (uint8_t *)malloc(vfs_size);
  std::unique_ptr<uint8_t> vfs_data_holder(new uint8_t[vfs_size]);
  uint8_t *vfs_data = vfs_data_holder.get();
  sys_copy_from_task(parent, vfs_data, vfs_data_loc, vfs_size);
  printf("heap used: %u\n", user::GetHeapUsed());

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
  // free(vfs_data);

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
