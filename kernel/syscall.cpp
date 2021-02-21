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

RET_TYPE debug_read(char *c) { return serial::TryRead(*c); }

RET_TYPE create_user_task(void *entry, uint32_t codesize, void *arg,
                          uint32_t *handle) {
  // FIXME: This is a raw pointer passed to userspace that will remain free
  // unless the user explicitly makes a syscall that frees this. This should not
  // be the case. We should provide some way to free it and retain ownership in
  // kernel space.
  auto *child = new UserTask(reinterpret_cast<TaskFunc>(entry), codesize, arg);
  *handle = reinterpret_cast<uint32_t>(child);
  return 0;
}

RET_TYPE destroy_user_task(uint32_t handle) {
  // We can safely cast to a UserTask here because this pointer originally was
  // created as a UserTask.
  auto *task = reinterpret_cast<UserTask *>(handle);
  assert(task->isUserTask());

  // Temporarily enable tasks here to allow for the destructor to call Join.
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

// TODO: It would be better instead if we returned some abstract representation
// of an allocation rather than specifically allocating a page. IE. something
// closer to:
//
//   RET_TYPE map_memory(void *addr, size_t size, uint8_t flags);
RET_TYPE map_page(void *vaddr) {
  if (!Is4MPageAligned(vaddr)) return -1;

  auto &pd = GetCurrentTask()->getPageDirectory();
  if (pd.isVirtualMapped(vaddr)) return -2;

  // FIXME: We skip the first 4MB because we could still need to read stuff
  // that multiboot inserted in the first 4MB page. Starting from 0 here could
  // lead to overwriting that multiboot data. We should probably copy that
  // data somewhere else after paging is enabled.
  uint8_t *paddr = GetPhysicalBitmap4M().NextFreePhysicalPage(/*start=*/1);
  if (!paddr) return -3;  // No physical addr available.

  pd.AddPage(vaddr, paddr, /*flags=*/PG_USER);
  return 0;
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
  regs->eax = ret;
}

}  // namespace

void InitializeSyscalls() {
  RegisterInterruptHandler(kSyscallInterrupt, SyscallHandler);
}
