// TODO: This stack isn't used right now, but user code should be in charge of
// setting up their own stack.
  .section .bss
  .align 16
stack_bottom:
  .skip 16384  // 16 KB
stack_top:

// This is space we allocate exclusively for using as a heap during bringup.
// We use this pre-allocated space as a brief substitute for an actual heap.
  .section .bss
  .global heap_bottom
  .align 16
heap_bottom:
// NOTE: This must be large enough to hold the initial ramdisk.
  .skip 65536  // 64 KB
  .global heap_top
heap_top:

  .section .user_program_entry
  .type __user_program_entry, @function
__user_program_entry:
  // ESP points to a page mapped by the kernel to store the initial stack.
  // Here we can access any arguments passed to the user by the kernel.

  // Jump to user-defined main.
  call __user_main

  // Call the task exit syscall and wait.
  mov $1, %eax
  int $0x80
  jmp .
