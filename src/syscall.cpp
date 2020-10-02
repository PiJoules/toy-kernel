#include <Terminal.h>
#include <kassert.h>
#include <ktask.h>
#include <syscall.h>

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

DEF_SYSCALL1(terminal_write, 0, const char *)
DEF_SYSCALL0(exit_user_task, 1)

namespace {

constexpr uint8_t kSyscallInterrupt = 0x80;

uint32_t terminal_write(const char *str) {
  terminal::Write(str);
  return 0;
}

uint32_t exit_user_task() {
  exit_this_task();
  return 0;
}

void *kSyscalls[] = {
    reinterpret_cast<void *>(terminal_write),
    reinterpret_cast<void *>(exit_user_task),
};
constexpr size_t kNumSyscalls = sizeof(kSyscalls) / sizeof(*kSyscalls);

void SyscallHandler(registers_t *regs) {
  auto syscall_num = regs->eax;
  assert(syscall_num < kNumSyscalls && "Invalid syscall!");
  void *syscall = kSyscalls[syscall_num];

  // We don't know how many parameters the function wants, so we just push them
  // all to the stack in the correct order. The function will use all the
  // parameters it wants and we can pop them all back off afterwards.
  uint32_t ret = 0;
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
