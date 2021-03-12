  .global _start
  .section ._start, "ax"
  .type _start, @function
_start:
  // ESP points to a page mapped by the kernel to store the initial stack.
  // Here we can access any arguments passed to the user by the kernel on a
  // task creation. main() wants an int and char** though, so we can just push
  // these values.
  pushl %esp  // argv
  pushl $1  // argc

  // pre_main() sets up stuff you would normally expect to be useable by main(),
  // like the heap.
  call pre_main
  test %eax, %eax
  jnz end

  call main

  add $8, %esp

  // Call the task exit syscall and wait.
  mov $1, %eax
end:
  int $0x80
  jmp .
