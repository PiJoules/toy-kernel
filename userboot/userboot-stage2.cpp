#include <_syscalls.h>
#include <elf.h>
#include <umalloc.h>
#include <userboot.h>
#include <vfs.h>
#include <vfs_helpers.h>

#include <cstdio>
#include <cstring>
#include <tuple>

namespace {

// The CmdFunc should return true if we should exit userboot after it is run.
using CmdFunc = bool (*)();
using CmdInfo = std::tuple<const char *, const char *, CmdFunc>;

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

int main([[maybe_unused]] int argc, [[maybe_unused]] char **argv) {
  printf("\n=== USERBOOT STAGE 2 ===\n\n");
  printf(
      "  This contains the rest of the userboot code. Userboot exists to run \n"
      "  a few test programs and launch into a user shell that can be used\n"
      "  for running various programs.\n\n");

  // Check starting a user task via syscall.
  printf("Trying test_user_program.bin ...\n");
  const vfs::Directory &initrd_dir = GetRootDir();
  auto handle1 = RunFlatUserBinary(initrd_dir, "test_user_program.bin");
  auto handle2 = RunFlatUserBinary(initrd_dir, "test_user_program.bin");
  sys::DestroyTask(handle1);
  sys::DestroyTask(handle2);
  printf("Finished test_user_program.bin.\n");

  if (const vfs::File *file = initrd_dir.getFile("shell")) {
    LoadElfProgram(file->getContents().data(), GetGlobalEnvInfo());
  } else {
    printf("WARN: Missing shell. Exiting early.\n");
  }

  return 0;
}
