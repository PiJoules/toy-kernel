// TODO: This stack isn't used right now, but user code should be in charge of
// setting up their own stack.
  .section .bss
  .align 16
stack_bottom:
  .skip 16384  // 16 KB
stack_top:

  .section .user_program_entry
  .type __user_program_entry, @function
__user_program_entry:
  // ESP points to a page mapped by the kernel to store the initial stack.
  // Here we can access any arguments passed to the user by the kernel.
  push %esp

  // Jump to user-defined main.
  call __user_main

  add $4, %esp

  // Call the task exit syscall and wait.
  mov $1, %eax
  int $0x80
  jmp .
