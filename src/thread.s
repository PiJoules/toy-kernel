// This assembly file exists to switch between tasks.
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
// It's important that if `task_t` or `ThreadNode` are changed, this file
// should be double-checked to account for relevant changes.

// There are 4 states we can be in at this point:
//
// 1) No privilege change, and the new task will be running for the first time.
//    For this case, we can just `ret` into the function after setting `esp`,
//    which should contain:
//    [esp+8] task func argument
//    [esp+4] task exit function
//    [esp]   task function
//
// 2) No privilege change, and we will be jumping into an already running task.
//    For this case, we should've previously stored the `esp` in this function.
//    After setting the `esp`, we can `ret` normally back into `schedule` and
//    proceed normally back up the call stack to the interrupt handler, where we
//    will `iret` with this new stack's values for EIP, CS, EFLAGS.
//
// 3) Privilege change, and the new task will be running for the first time.
//    For this case, we can similarly jump into the task function as in (2),
//    but this will require an `iret`, and the initial stack will need:
//    [esp+16] SS
//    [esp+12] Stack pointer
//    [esp+8]  EFLAGS
//    [esp+4]  CS
//    [esp]    task function
//
// 4) Privilege change, and we will be jumping into an already running task.
//    For this case, we will still need to use `iret` since we are changing
//    privileges, but we can still use the stack from the previously saved esp.

.macro SWAP_TASKS
  // Set the new task passed as an argument as the current task.
  movl 4(%esp),%eax         // Get the new task as the 1st arg (task_t *)

  // Set the registers pased off values stored in the new task.
  movl 0(%eax),%esp
  movl 4(%eax),%ebp
  movl 8(%eax),%ebx
  movl 12(%eax),%esi
  movl 16(%eax),%edi

  // Restore segment registers.
  movw 28(%eax), %ds
  movw 30(%eax), %es
  movw 32(%eax), %fs
  movw 34(%eax), %gs
.endm

  .global switch_kernel_task_run
switch_kernel_task_run:
  SWAP_TASKS
  pushl 20(%eax) // eflags

  mov 36(%eax), %cx  // cs
  movzx %cx, %ecx
  push %ecx

  pushl 24(%eax)  // eip
  iret

  .global switch_first_kernel_task_run
switch_first_kernel_task_run:
  SWAP_TASKS
  iret

  .global switch_first_user_task_run
switch_first_user_task_run:
  SWAP_TASKS
  iret

  .globl switch_user_task_run
switch_user_task_run:
  SWAP_TASKS
  push %ds  // DS/SS

  pushl 0(%eax)  // esp
  pushl 20(%eax) // eflags

  mov 36(%eax), %cx
  movzx %cx, %ecx
  push %ecx   // cs

  pushl 24(%eax)  // eip
  iret
