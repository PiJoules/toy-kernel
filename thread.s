// This assembly file exists to switch between threads.
// It's better to have it in its own assembly file and not as inline assembly
// since the compiler may use clobbered registers. For example, given:
//
//   asm volatile("mov %0, %%esp" : : "r" (next->esp) : "esp");
//   asm volatile("mov %0, %%ebp" : : "r" (next->ebp) : "ebp");
//
// We cannot reliably use variables like `next` after this because (at
// least without optimizations), variables can be accessed by loading relative
// to ebp or esp. Even if we don't perform any more accesses after this, the
// compiler can still do stuff like adjust esp at the end of the function (in
// the event local variables are used).
//
// It's important that if `thread_t` or `ThreadNode` are changed, this file
// should be double-checked to account for relevant changes.

// There are 4 states we can be in at this point:
//
// 1) No privilege change, and the new thread will be running for the first time.
//    For this case, we can just `ret` into the function after setting `esp`,
//    which should contain:
//    [esp+8] thread func argument
//    [esp+4] thread exit function
//    [esp]   thread function
//
// 2) No privilege change, and we will be jumping into an already running thread.
//    For this case, we should've previously stored the `esp` in this function.
//    After setting the `esp`, we can `ret` normally back into `schedule` and
//    proceed normally back up the call stack to the interrupt handler, where we
//    will `iret` with this new stack's values for EIP, CS, EFLAGS.
//
// 3) Privilege change, and the new thread will be running for the first time.
//    For this case, we can similarly jump into the thread function as in (2),
//    but this will require an `iret`, and the initial stack will need:
//    [esp+16] SS
//    [esp+12] Stack pointer
//    [esp+8]  EFLAGS
//    [esp+4]  CS
//    [esp]    thread function
//
// 4) Privilege change, and we will be jumping into an already running thread.
//    For this case, we will still need to use `iret` since we are changing
//    privileges, but we can still use the stack from the previously saved esp.

.macro SWAP_THREADS
  // Set the new thread passed as an argument as the current thread.
  movl 4(%esp),%eax         // Get the new thread as the 1st arg (thread_t *)
  movl %eax,(CurrentThread) // Set the current thread node to it.

  // Set the registers pased off values stored in the new thread.
  movl 0(%eax),%esp
  movl 4(%eax),%ebp
  movl 8(%eax),%ebx
  movl 12(%eax),%esi
  movl 16(%eax),%edi

  // Restore segment registers.
  movw 24(%eax), %ds
  movw 26(%eax), %es
  movw 28(%eax), %fs
  movw 30(%eax), %gs
.endm

  .global switch_kernel_thread_run
switch_kernel_thread_run:
  SWAP_THREADS
  pushl 20(%eax) // eflags

  mov 40(%eax), %cx
  movzx %cx, %ecx
  push %ecx

  pushl 36(%eax)
  iret

  .global switch_first_kernel_thread_run
switch_first_kernel_thread_run:
  SWAP_THREADS
  iret

  .global switch_first_user_thread_run
switch_first_user_thread_run:
  SWAP_THREADS
  movw $0x23, %cx
  movw %cx, %ds
  movw %cx, %es
  movw %cx, %fs
  movw %cx, %gs
  iret

  .globl switch_user_thread_run
switch_user_thread_run:
  SWAP_THREADS
  movw 24(%eax), %cx
  mov %cx, %ds
  mov %cx, %es
  mov %cx, %fs
  mov %cx, %gs

  movzx %cx, %ecx
  push %ecx // DS

  pushl 0(%eax)  // esp
  pushl 20(%eax) // eflags

  mov 40(%eax), %cx
  movzx %cx, %ecx
  push %ecx   // cs

  pushl 36(%eax)  // eip
  iret
