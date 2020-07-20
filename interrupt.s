.macro ISR_NOERRCODE isr_num
  .globl isr\isr_num
isr\isr_num:
  cli
  push $0
  push $\isr_num
  jmp isr_common_stub
.endm

.macro ISR_ERRCODE isr_num
  .globl isr\isr_num
isr\isr_num:
  cli
  push $\isr_num
  jmp isr_common_stub
.endm

.macro IRQ irq_num interrupt_num
  .globl irq\irq_num
irq\irq_num:
  cli
  push $0
  push $\interrupt_num
  jmp irq_common_stub
.endm

// See https://wiki.osdev.org/Exceptions for exceptions that have error codes.
ISR_NOERRCODE 0
ISR_NOERRCODE 1
ISR_NOERRCODE 2
ISR_NOERRCODE 3
ISR_NOERRCODE 4
ISR_NOERRCODE 5
ISR_NOERRCODE 6
ISR_NOERRCODE 7
ISR_ERRCODE   8
ISR_NOERRCODE 9
ISR_ERRCODE   10
ISR_ERRCODE   11
ISR_ERRCODE   12
ISR_ERRCODE   13
ISR_ERRCODE   14
ISR_NOERRCODE 15
ISR_NOERRCODE 16
ISR_ERRCODE   17
ISR_NOERRCODE 18
ISR_NOERRCODE 19
ISR_NOERRCODE 20
ISR_NOERRCODE 21
ISR_NOERRCODE 22
ISR_NOERRCODE 23
ISR_NOERRCODE 24
ISR_NOERRCODE 25
ISR_NOERRCODE 26
ISR_NOERRCODE 27
ISR_NOERRCODE 28
ISR_NOERRCODE 29
ISR_ERRCODE   30
ISR_NOERRCODE 31
ISR_NOERRCODE 128

.macro SAVE_REGISTERS
  pusha                    // Pushes eax,ecx,edx,ebx,esp,ebp,esi,edi

  mov %ds, %ax               // Lower 16-bits of eax = ds.
  push %eax                 // save the data segment descriptor

  mov $0x10, %ax  // load the kernel data segment descriptor
  mov %ax, %ds
  mov %ax, %es
  mov %ax, %fs
  mov %ax, %gs

  // Push a pointer to the current top of stack. This becomes the
  // registers_t* parameter.
  push %esp
.endm

.macro RESTORE_REGISTERS
  add $4, %esp  // Remove the registers_t* parameter.

  pop %eax        // reload the original data segment descriptor
  mov %ax, %ds
  mov %ax, %es
  mov %ax, %fs
  mov %ax, %gs

  popa                     // Pops edi,esi,ebp...
  add $8, %esp     // Cleans up the pushed error code and pushed ISR number
  sti
  iret           // pops 5 things at once: CS, EIP, EFLAGS, SS, and ESP
.endm

// This is our common ISR stub. It saves the processor state, sets
// up for kernel mode segments, calls the C-level fault handler,
// and finally restores the stack frame.
isr_common_stub:
  SAVE_REGISTERS
  call isr_handler
  RESTORE_REGISTERS

IRQ 0, 32
IRQ 1, 33
IRQ 2, 34
IRQ 3, 35
IRQ 4, 36
IRQ 5, 37
IRQ 6, 38
IRQ 7, 39
IRQ 8, 40
IRQ 9, 41
IRQ 10, 42
IRQ 11, 43
IRQ 12, 44
IRQ 13, 45
IRQ 14, 46
IRQ 15, 47

// This is our common IRQ stub. It saves the processor state, sets
// up for kernel mode segments, calls the C-level fault handler,
// and finally restores the stack frame.
irq_common_stub:
  SAVE_REGISTERS
  call irq_handler
  RESTORE_REGISTERS
