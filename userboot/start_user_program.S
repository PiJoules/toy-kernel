  .global __user_program_entry
  .section .user_program_entry, "ax"
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
