#include <_syscalls.h>
#include <elf.h>
#include <umalloc.h>
#include <userboot.h>
#include <vfs.h>

#include <cstdio>
#include <cstring>
#include <tuple>

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

// The CmdFunc should return true if we should exit userboot after it is run.
using CmdFunc = bool (*)();
using CmdInfo = std::tuple<const char *, const char *, CmdFunc>;

bool DumpCommands();
bool Shutdown() { return true; }

vfs::Directory *FileSystem;
size_t VFSSize;
uint8_t *VFSStart;

bool RunTests() {
  if (const vfs::File *file = FileSystem->getFile("userspace-tests"))
    LoadElfProgram(file->getContents().data());
  return false;
}

bool RunHelloWorld() {
  if (const vfs::File *file = FileSystem->getFile("hello-world"))
    LoadElfProgram(file->getContents().data());
  return false;
}

bool RunHelloWorldPIC() {
  if (const vfs::File *file = FileSystem->getFile("hello-world-PIC"))
    LoadElfProgram(file->getContents().data());
  return false;
}

bool RunHelloWorldPICStatic() {
  if (const vfs::File *file = FileSystem->getFile("hello-world-PIC-static"))
    LoadElfProgram(file->getContents().data());
  return false;
}

bool RunShell() {
  if (const vfs::File *file = FileSystem->getFile("shell")) {
    std::unique_ptr<uint8_t> alloc(new uint8_t[sizeof(VFSSize) + VFSSize]);
    memcpy(alloc.get(), &VFSSize, sizeof(VFSSize));
    memcpy(alloc.get() + sizeof(VFSSize), VFSStart, VFSSize);
    printf("Expected vfs start: %p\n", alloc.get() + sizeof(VFSSize));
    LoadElfProgram(file->getContents().data(), alloc.get());
  }
  return true;
}

constexpr CmdInfo kCmds[] = {
    {"help", "Dump commands.", DumpCommands},
    {"shutdown", "Exit userboot.", Shutdown},
    {"invalid-opcode", "Trigger an invalid opcode exception",
     TriggerInvalidOpcodeException},
    {"runtests", "Run userspace tests", RunTests},
    {"hello-world", "Run hello world program", RunHelloWorld},
    {"shell", "Launch the shell", RunShell},
    {"hello-world-pic", "Run PIC hello world program", RunHelloWorldPIC},
    {"hello-world-pic-static", "Run PIC hello world program",
     RunHelloWorldPICStatic},
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

}  // namespace

int main(int argc, char **argv) {
  printf("\n=== USERBOOT STAGE 2 ===\n\n");
  printf(
      "  This contains the rest of the userboot code. Userboot just simply \n"
      "  launches a user shell that can be used for running various \n"
      "  programs.\n\n");

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
  vfs::Directory *initrd_dir = vfs->getDir("initrd_files");
  auto handle1 = RunFlatUserBinary(*initrd_dir, "test_user_program.bin");
  auto handle2 = RunFlatUserBinary(*initrd_dir, "test_user_program.bin");
  sys::DestroyTask(handle1);
  sys::DestroyTask(handle2);
  printf("Finished test_user_program.bin.\n");

  FileSystem = initrd_dir;
  VFSSize = initrd_size;
  VFSStart = initrd_data;

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
