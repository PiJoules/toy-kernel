  .section .bss
  .align 16
stack_bottom:
  .skip 16384  # 16 KB
stack_top:

  .section .user_program_entry
  .type __user_program_entry, @function
__user_program_entry:
  // Setup the stack.
  // FIXME: This clobbers any arguments we may want to pass from a task
  // creation. We should probably instead allocate some memory region that we
  // can copy our task arguments and read from there.
  mov $stack_top, %esp

  // Jump to user-defined main.
  call __user_main

  // Call user_thread_exit().
  mov $1, %eax
  int $0x80
  jmp .
