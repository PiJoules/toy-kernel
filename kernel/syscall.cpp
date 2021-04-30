#include <assert.h>
#include <kernel.h>
#include <ktask.h>
#include <syscall.h>

#define SYSCALL_INT "0x80"
#define RET_TYPE int32_t

namespace {

constexpr uint8_t kSyscallInterrupt = 0x80;

RET_TYPE debug_write(const char *str) {
  DebugPrint(str);
  return 0;
}

RET_TYPE exit_user_task() {
  exit_this_task();
  return 0;
}

RET_TYPE debug_read(char *c) {
  if (serial::TryRead(*c)) return 0;
  return 1;
}

RET_TYPE create_user_task(void *entry, uint32_t codesize, void *arg,
                          uint32_t *handle, uint32_t entry_offset) {
  // FIXME: This is a raw pointer passed to userspace that will remain free
  // unless the user explicitly makes a syscall that frees this. This should not
  // be the case. We should provide some way to free it and retain ownership in
  // kernel space.
  auto *child = new UserTask(reinterpret_cast<TaskFunc>(entry), codesize, arg,
                             /*copyfunc=*/UserTask::CopyArgDefault,
                             /*entry_offset=*/entry_offset);
  *handle = reinterpret_cast<uint32_t>(child);
  return 0;
}

RET_TYPE destroy_user_task(uint32_t handle) {
  // We can safely cast to a UserTask here because this pointer originally was
  // created as a UserTask.
  auto *task = reinterpret_cast<UserTask *>(handle);
  assert(task->isUserTask());

  // Temporarily enable tasks here to allow for the destructor to call Join.
  // FIXME: This should not be explicitly set here.
  EnableInterrupts();
  delete task;
  DisableInterrupts();

  return 0;
}

RET_TYPE copy_from_task(uint32_t handle, void *dst, const void *src,
                        size_t size) {
  auto *task = reinterpret_cast<UserTask *>(handle);
  assert(task->isUserTask());
  task->Read(dst, src, size);
  return 0;
}

RET_TYPE get_parent_task(uint32_t *handle) {
  *handle = reinterpret_cast<uint32_t>(GetCurrentTask()->getParent());
  return 0;
}

RET_TYPE get_parent_task_id(uint32_t *id) {
  *id = GetCurrentTask()->getParent()->getID();
  return 0;
}

#define MAP_SUCCESS (0)
#define MAP_UNALIGNED_ADDR (-1)
#define MAP_ALREADY_MAPPED (-2)
#define MAP_OOM (-3)

// TODO: It would be better instead if we returned some abstract representation
// of an allocation rather than specifically allocating a page. IE. something
// closer to:
//
//   RET_TYPE map_memory(void *addr, size_t size, uint8_t flags);
RET_TYPE map_page(void *vaddr) {
  if (!Is4MPageAligned(vaddr)) return MAP_UNALIGNED_ADDR;

  auto &pd = GetCurrentTask()->getPageDirectory();
  if (pd.isVirtualMapped(vaddr)) return MAP_ALREADY_MAPPED;

  // FIXME: Note that if we allow the zero-th page, we will be returning NULL
  // from here effectively. We should have a separate way of returning a failure
  // state separate from the pointer returned.
  uint8_t *paddr = GetPhysicalBitmap4M().NextFreePhysicalPage(/*start=*/1);
  if (!paddr) return MAP_OOM;  // No physical addr available.

  pd.AddPage(vaddr, paddr, /*flags=*/PG_USER);
  return MAP_SUCCESS;
}

void *kSyscalls[] = {
    reinterpret_cast<void *>(debug_write),
    reinterpret_cast<void *>(exit_user_task),
    reinterpret_cast<void *>(debug_read),
    reinterpret_cast<void *>(create_user_task),
    reinterpret_cast<void *>(destroy_user_task),
    reinterpret_cast<void *>(copy_from_task),
    reinterpret_cast<void *>(get_parent_task),
    reinterpret_cast<void *>(get_parent_task_id),
    reinterpret_cast<void *>(map_page),
};
constexpr size_t kNumSyscalls = sizeof(kSyscalls) / sizeof(*kSyscalls);

void SyscallHandler(X86Registers *regs) {
  assert(GetCurrentTask()->isUserTask() &&
         "Should not call syscalls from a kernel task.");
  auto syscall_num = regs->eax;
  assert(syscall_num < kNumSyscalls && "Invalid syscall!");
  void *syscall = kSyscalls[syscall_num];

  // We don't know how many parameters the function wants, so we just push them
  // all to the stack in the correct order. The function will use all the
  // parameters it wants and we can pop them all back off afterwards.
  RET_TYPE ret = 0;
  asm volatile(
      "\
    push %1 \n \
    push %2 \n \
    push %3 \n \
    push %4 \n \
    push %5 \n \
    call *%6 \n \
    pop %%ebx \n \
    pop %%ebx \n \
    pop %%ebx \n \
    pop %%ebx \n \
    pop %%ebx"
      : "=a"(ret)
      : "r"(regs->edi), "r"(regs->esi), "r"(regs->edx), "r"(regs->ecx),
        "r"(regs->ebx), "r"(syscall));
  regs->eax = static_cast<uint32_t>(ret);
}

}  // namespace

void InitializeSyscalls() {
  RegisterInterruptHandler(kSyscallInterrupt, SyscallHandler);
}
