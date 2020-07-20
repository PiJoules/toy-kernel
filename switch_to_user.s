  .globl jump_usermode
jump_usermode:
  mov $0x23, %ax
  mov %ax, %ds
  mov %ax, %es
  mov %ax, %fs
  mov %ax, %gs

  // Don't worry about handling SS since it's handled by iret.

  mov %esp, %eax

  // User data segment with bottom 2 bits set for ring 3.
  // 0x20: User data segment (0x20 | 0x3 = 0x23)
  push $0x23
  push %eax  // Push our current esp for the iret stack frame.
  pushf

  // User code segment with bottom 2 bits set for ring 3.
  // 0x18: User code segment (0x18 | 0x3 = 0x1b)
  push $0x1b

  // Jump to a function that will operate in user mode!
  push user_mode_function

  iret
