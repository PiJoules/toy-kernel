#include <assert.h>
#include <kernel.h>
#include <ktask.h>
#include <syscall.h>

#define SYSCALL_INT "0x80"
#define RET_TYPE int32_t

#define DEF_SYSCALL0(func, syscall)                             \
  RET_TYPE syscall_##func() {                                   \
    RET_TYPE a;                                                 \
    asm volatile("int $" SYSCALL_INT : "=a"(a) : "0"(syscall)); \
    return a;                                                   \
  }

#define DEF_SYSCALL1(func, syscall, P1)                                \
  RET_TYPE syscall_##func(P1 p1) {                                     \
    RET_TYPE a;                                                        \
    asm volatile("int $" SYSCALL_INT                                   \
                 : "=a"(a)                                             \
                 : "0"(syscall), "b"(reinterpret_cast<uint32_t>(p1))); \
    return a;                                                          \
  }

#define DEF_SYSCALL2(func, syscall, P1, P2)                           \
  RET_TYPE syscall_##func(P1 p1, P2 p2) {                             \
    RET_TYPE a;                                                       \
    asm volatile("int $" SYSCALL_INT                                  \
                 : "=a"(a)                                            \
                 : "0"(syscall), "b"(reinterpret_cast<uint32_t>(p1)), \
                   "c"(reinterpret_cast<uint32_t>(p2)));              \
    return a;                                                         \
  }

#define DEF_SYSCALL3(func, syscall, P1, P2, P3)                       \
  RET_TYPE syscall_##func(P1 p1, P2 p2, P3 p3) {                      \
    RET_TYPE a;                                                       \
    asm volatile("int $" SYSCALL_INT                                  \
                 : "=a"(a)                                            \
                 : "0"(syscall), "b"(reinterpret_cast<uint32_t>(p1)), \
                   "c"(reinterpret_cast<uint32_t>(p2)),               \
                   "d"(reinterpret_cast<uint32_t>(p3)));              \
    return a;                                                         \
  }

#define DEF_SYSCALL4(func, syscall, P1, P2, P3, P4)                   \
  RET_TYPE syscall_##func(P1 p1, P2 p2, P3 p3, P4 p4) {               \
    RET_TYPE a;                                                       \
    asm volatile("int $" SYSCALL_INT                                  \
                 : "=a"(a)                                            \
                 : "0"(syscall), "b"(reinterpret_cast<uint32_t>(p1)), \
                   "c"(reinterpret_cast<uint32_t>(p2)),               \
                   "d"(reinterpret_cast<uint32_t>(p3)),               \
                   "S"(reinterpret_cast<uint32_t>(p4)));              \
    return a;                                                         \
  }

#define DEF_SYSCALL5(func, syscall, P1, P2, P3, P4, P5)               \
  RET_TYPE syscall_##func(P1 p1, P2 p2, P3 p3, P4 p4, P5 p5) {        \
    RET_TYPE a;                                                       \
    asm volatile("int $" SYSCALL_INT                                  \
                 : "=a"(a)                                            \
                 : "0"(syscall), "b"(reinterpret_cast<uint32_t>(p1)), \
                   "c"(reinterpret_cast<uint32_t>(p2)),               \
                   "d"(reinterpret_cast<uint32_t>(p3)),               \
                   "S"(reinterpret_cast<uint32_t>(p4)),               \
                   "D"(reinterpret_cast<uint32_t>(p5)));              \
    return a;                                                         \
  }

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

void *kSyscalls[] = {
    reinterpret_cast<void *>(debug_write),
    reinterpret_cast<void *>(exit_user_task),
    reinterpret_cast<void *>(debug_read),
    reinterpret_cast<void *>(create_user_task),
    reinterpret_cast<void *>(destroy_user_task),
    reinterpret_cast<void *>(copy_from_task),
    reinterpret_cast<void *>(get_parent_task),
    reinterpret_cast<void *>(get_parent_task_id),
};
constexpr size_t kNumSyscalls = sizeof(kSyscalls) / sizeof(*kSyscalls);

void SyscallHandler(X86Registers *regs) {
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
