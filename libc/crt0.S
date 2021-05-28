  .global _start
  .section ._start, "ax"
  .type _start, @function
_start:
  // ESP points to a page mapped by the kernel to store the initial stack.
  // Here we can access any arguments passed to the user by the kernel on a
  // task creation. main() wants an int and char** though, so we can just push
  // these values.
  // NOTE that for any elf binary, this value is always the pointer to the
  // arginfo struct passed to the create task syscall in `elf.cpp`.
  pushl %esp  // argv

  // pre_main() sets up stuff you would normally expect to be useable by main(),
  // like the heap. We also call main from there.
  call pre_main@PLT
  test %eax, %eax
  jnz end

  add $4, %esp

  // Call the task exit syscall and wait.
  mov $1, %eax
end:
  int $0x80
  jmp .
