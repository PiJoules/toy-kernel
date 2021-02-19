  .global _start
  .section ._start
  .type _start, @function
_start:
  // ESP points to a page mapped by the kernel to store the initial stack.
  // Here we can access any arguments passed to the user by the kernel on a
  // task creation. main() wants an int and char** though, so we can just push
  // these values.
  pushl %esp  // argv
  pushl $1  // argc

  call main

  add $8, %esp

  // Call the task exit syscall and wait.
  mov $1, %eax
  int $0x80
  jmp .
