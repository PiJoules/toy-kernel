/**
 * This contains code that should run before the start of main.
 */

#include <_syscalls.h>
#include <elf.h>
#include <limits.h>
#include <umalloc.h>

#include <cstdio>
#include <cstring>

// NOTE: This should always have the same value as USER_START in the kernel's
// paging.h.
#define USER_START UINT32_C(0x40000000)  // 1GB

// FIXME: This should only be accessible within libc.
const void *__raw_vfs_data;

namespace {

Handle __raw_vfs_data_owner;

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

}  // namespace

extern "C" int main(int, char **);
extern "C" Handle GetRawVFSDataOwner() { return __raw_vfs_data_owner; }

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
  __raw_vfs_data = arginfo.raw_vfs_data;
  __raw_vfs_data_owner = arginfo.raw_vfs_data_owner;

  char packed_argv[arginfo.packed_argv_size];
  sys_copy_from_task(parent, packed_argv, arginfo.packed_argv,
                     arginfo.packed_argv_size);
  char *argv[ARG_MAX];
  int argc = UnpackArgv(arginfo.packed_argv_size, packed_argv, argv);
  return main(argc, argv);
}
