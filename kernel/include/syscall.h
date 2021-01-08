#ifndef SYSCALL_H_
#define SYSCALL_H_

#define SYSCALL_INT "0x80"
#define RET_TYPE uint32_t

#define DECL_SYSCALL0(func) RET_TYPE syscall_##func();
#define DECL_SYSCALL1(func, P1) RET_TYPE syscall_##func(P1 p1);

DECL_SYSCALL1(debug_write, const char *)
DECL_SYSCALL0(exit_user_task)

void InitializeSyscalls();

#endif
