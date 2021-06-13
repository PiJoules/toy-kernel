/**
 * This contains code that should run before the start of main.
 */

#include <_syscalls.h>
#include <elf.h>
#include <limits.h>
#include <umalloc.h>
#include <vfs_helpers.h>

#include <cstdio>
#include <cstring>

// NOTE: This should always have the same value as USER_START in the kernel's
// paging.h.
#define USER_START UINT32_C(0x40000000)  // 1GB

namespace {

GlobalEnvInfo kGlobalEnvInfo;
vfs::Directory *kRootVFS;
vfs::Directory *CWD;

uint32_t PageIndex4M(const void *addr) { return (uint32_t)(addr) >> 22; }
void *PageAddr4M(uint32_t page) { return (void *)(page << 22); }

// Attempt to get the next available page by getting the location of the current
// binary in memory, then getting the page after it. Note that this only works
// because binaries right now fit into a 4MB page.
void *NextPage() {
  uint32_t page = PageIndex4M((void *)&NextPage);
  return PageAddr4M(page + 1);
}

constexpr const int kExitFailure = -1;
constexpr const size_t kInitHeapSize = kPageSize4M;

// This assumes the heap is setup to support unique_ptr.
std::unique_ptr<vfs::Directory> ParseUSTARFromRawData() {
  Handle vfs_owner = GetRawVFSDataOwner();

  // The first argument will be a pointer to vfs data in the owner task's
  // address space. Map some virtual address in this task's address space to
  // the page with the vfs so we can read from it and construct a vfs object.
  auto vfs_loc_int = reinterpret_cast<uintptr_t>(GetRawVFSData());
  uint32_t loc_offset = vfs_loc_int % kPageSize4M;
  auto vfs_loc_page = vfs_loc_int - loc_offset;
  uint8_t *this_vfs_page;
  sys_share_page(vfs_owner, reinterpret_cast<void **>(&this_vfs_page),
                 reinterpret_cast<void *>(vfs_loc_page));
  uint8_t *this_vfs = this_vfs_page + loc_offset;

  // FIXME: It could be possible that initrd is more than a page in size. In
  // this case, we should check for it and be sure to map the remaining pages.
  std::unique_ptr<vfs::Directory> vfs = vfs::ParseUSTAR(this_vfs);

  sys_unmap_page(this_vfs_page);

  return vfs;
}

}  // namespace

extern "C" int main(int, char **);
extern "C" const GlobalEnvInfo *GetGlobalEnvInfo() { return &kGlobalEnvInfo; }
extern "C" Handle GetRawVFSDataOwner() {
  return kGlobalEnvInfo.raw_vfs_data_owner;
}
extern "C" const void *GetRawVFSData() { return kGlobalEnvInfo.raw_vfs_data; }

const vfs::Directory &GetRootDir() { return *kRootVFS; }

const vfs::Directory &GetCWD() { return *CWD; }

// Populate argv with pointers into packed_argv. Return the number of arguments
// in argv (which will be argc).
int UnpackArgv(size_t packed_argv_size, char *packed_argv,
               char *argv[ARG_MAX]) {
  int argc = 0;
  for (size_t i = 0; i < packed_argv_size;) {
    *(argv++) = packed_argv;
    size_t len = strlen(packed_argv);
    packed_argv += len + 1;  // +1 to skip past the null terminator.
    i += len + 1;
    ++argc;
  }
  return argc;
}

// FIXME: Get rid of the arguments since these aren't used.
extern "C" int pre_main(void **arg_ptr) {
  void *heap_start = NextPage();
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

  uint8_t *heap_bottom = (uint8_t *)heap_start;
  uint8_t *heap_top = heap_bottom + kInitHeapSize;
  user::InitializeUserHeap(heap_bottom, heap_top);

  // The argument passed here is the ArgInfo struct prepared in elf.cpp. Because
  // the arginfo is on the stack of the parent process, we must gain permission
  // to read from the parent address space.
  //
  // NOTE: We can read from the arginfo in this manner because within elf.cpp,
  // we explicitly do not destroy the parent task that spawned this task, nor do
  // we finish exiting the function where this task was spawned until this
  // subtask has finished executing. If we were to do a non-blocking task spawn,
  // then we would require a different synchonous mechanism for reading from the
  // parent.
  ArgInfo arginfo;
  Handle parent = sys_get_parent_task();
  {
    void *arginfo_stack_addr = *arg_ptr;
    auto arginfo_int = reinterpret_cast<uintptr_t>(arginfo_stack_addr);
    uint32_t offset = arginfo_int % kPageSize4M;
    auto arginfo_page = arginfo_int - offset;
    uint8_t *this_arginfo_page;
    sys_share_page(parent, reinterpret_cast<void **>(&this_arginfo_page),
                   reinterpret_cast<void *>(arginfo_page));
    auto *this_arginfo =
        reinterpret_cast<ArgInfo *>(this_arginfo_page + offset);

    // Extract the arginfo members.
    memcpy(&arginfo, this_arginfo, sizeof(arginfo));

    sys_unmap_page(this_arginfo_page);
  }
  kGlobalEnvInfo.raw_vfs_data = arginfo.env_info.raw_vfs_data;
  kGlobalEnvInfo.raw_vfs_data_owner = arginfo.env_info.raw_vfs_data_owner;

  auto root_vfs = ParseUSTARFromRawData();
  kRootVFS = root_vfs.get();

  // Get the current working directory.
  if (!arginfo.pwd) {
    CWD = kRootVFS;
  } else {
    CWD = kRootVFS->getDir(arginfo.pwd);
    assert(CWD && "Could not find pwd.");
  }

  char packed_argv[arginfo.packed_argv_size];
  sys_copy_from_task(parent, packed_argv, arginfo.packed_argv,
                     arginfo.packed_argv_size);
  char *argv[ARG_MAX];
  int argc = UnpackArgv(arginfo.packed_argv_size, packed_argv, argv);
  return main(argc, argv);
}
